using ClaimTerritory;
using ShatteredFrontier.CraftingContent;
using ShatteredFrontier.InventoryLedger;
using Ledger = ShatteredFrontier.InventoryLedger.InventoryLedger;
using ShatteredFrontier.Placement;

var zonePath = args.Length == 2 ? args[0] : "content/world/zones/greenlands_silverbrook.json";
var recipePath = args.Length == 2 ? args[1] : "content/crafting/recipes_greenlands_starter.json";
var zone = ProtectedZone.Load(zonePath);
var claims = new ClaimService(zone, TimeProvider.System, _ => true);
var reserved = claims.Reserve("reserve", 0, "clm_home", "chr_owner", new Point(900, 900), 40);
Check(reserved.Accepted, "fixture claim");
var store = new InMemoryInventoryLedgerStore();
var ledger = new Ledger(store);
ledger.CreateContainer(new ContainerSpec("ctn_owner", OwnerKind.Character, "chr_owner", ContainerKind.Backpack, 100));
ledger.CreateContainer(new ContainerSpec("ctn_guest", OwnerKind.Character, "chr_guest", ContainerKind.Backpack, 100));
var recipes = RecipeCatalog.LoadFiles([recipePath]);
var placement = new PlacementService(claims, ledger, recipes);
try { placement.SetEffectiveSettings(new PlacementRefundSettings(101, 50, 25)); throw new Exception("accepted invalid refund setting"); }
catch (ArgumentOutOfRangeException) { Console.WriteLine("PASS bounded refund setting"); }
var kinds = Enum.GetValues<PieceKind>();
foreach (var kind in kinds)
{
    var name = kind.ToString().ToLowerInvariant();
    ledger.Grant("stock_" + name, "ctn_owner", ledger.Read("ctn_owner").Revision,
        "item/" + name + "_material_bundle", 4, "test_source", "fixture");
}
PlaceCommand Command(string op, PieceKind kind, int x, string actor = "chr_owner", long? revision = null) =>
    new(op, "clm_home", actor, actor == "chr_owner" ? "ctn_owner" : "ctn_guest",
        ledger.Read(actor == "chr_owner" ? "ctn_owner" : "ctn_guest").Revision,
        claims.Check("clm_home", actor, ClaimRight.Build).PermissionRevision,
        revision ?? placement.Revision("clm_home"), "str_" + op, kind,
        "piece/" + kind.ToString().ToLowerInvariant(), new Cell(x, 0, 0));
long Quantity(PieceKind kind) => ledger.Read("ctn_owner").Quantities.GetValueOrDefault(
    "item/" + kind.ToString().ToLowerInvariant() + "_material_bundle");
void Check(bool condition, string label) { if (!condition) throw new Exception(label); Console.WriteLine("PASS " + label); }

var denied = Command("denied", PieceKind.Foundation, 0, "chr_guest");
Check(placement.Place(denied).Reason == PlacementReason.PermissionDenied &&
    ReferenceEquals(placement.Place(denied), placement.Receipt("denied")) && Quantity(PieceKind.Foundation) == 4,
    "stable permission rejection without debit");
var missing = Command("missing", PieceKind.Foundation, 0) with { ContainerId = "ctn_guest" };
Check(placement.Place(missing).Reason == PlacementReason.PermissionDenied && placement.Structures("clm_home").Count == 0,
    "foreign inventory denied");
ledger.Consume("exhaust", "ctn_owner", ledger.Read("ctn_owner").Revision,
    "item/roof_material_bundle", 4, "test_sink");
var insufficient = Command("insufficient", PieceKind.Roof, 9);
Check(placement.Place(insufficient).Reason == PlacementReason.InsufficientMaterials &&
    placement.Structures("clm_home").Count == 0, "insufficient materials leave no structure");
var foundation = Command("foundation", PieceKind.Foundation, 0);
var winner = placement.Place(foundation);
Check(winner.State == PlacementState.Committed && Quantity(PieceKind.Foundation) == 3 &&
    ReferenceEquals(winner, placement.Place(foundation)), "place consumes once and retries same receipt");
Check(placement.Place(foundation with { Cell = new Cell(1, 0, 0) }).Reason == PlacementReason.OperationConflict,
    "changed operation payload rejected");
var stale = Command("stale", PieceKind.Wall, 1, revision: 0);
Check(placement.Place(stale).Reason == PlacementReason.StaleRevision && Quantity(PieceKind.Wall) == 4,
    "stale revision loses without debit");
var compensated = Command("compensated", PieceKind.Wall, 1);
var before = Quantity(PieceKind.Wall);
var aborted = placement.Place(compensated, failAfterConsume: true);
Check(aborted.State == PlacementState.Compensated && Quantity(PieceKind.Wall) == before &&
    placement.Structures("clm_home").Count == 1 && ReferenceEquals(aborted, placement.Place(compensated)),
    "post-consume failure compensates exactly once");
foreach (var (kind, x) in new[] { (PieceKind.Wall, 1), (PieceKind.Door, 2) })
    Check(placement.Place(Command(kind.ToString(), kind, x)).State == PlacementState.Committed,
        "placed " + kind);
ledger.Grant("restock_roof", "ctn_owner", ledger.Read("ctn_owner").Revision,
    "item/roof_material_bundle", 1, "test_source", "fixture");
Check(placement.Place(Command("roof", PieceKind.Roof, 3)).State == PlacementState.Committed &&
    placement.Structures("clm_home").Count == 4, "placed all four content pieces");
var remove = new RemoveCommand("remove_wall", "clm_home", "chr_owner", "ctn_owner",
    ledger.Read("ctn_owner").Revision, claims.Check("clm_home", "chr_owner", ClaimRight.Build).PermissionRevision,
    placement.Revision("clm_home"), "str_Wall", RemovalKind.Voluntary);
var wallBefore = Quantity(PieceKind.Wall);
var removal = placement.Remove(remove);
Check(removal.State == PlacementState.Committed && removal.MaterialDelta == 0 &&
    Quantity(PieceKind.Wall) == wallBefore && ReferenceEquals(removal, placement.Remove(remove)),
    "bounded fractional refund floors and replays once");
var staleRemove = remove with { OperationId = "remove_stale", StructureId = "str_Door" };
Check(placement.Remove(staleRemove).Reason == PlacementReason.StaleRevision,
    "stale removal rejected");
var undo = remove with { OperationId = "undo_foundation", StructureId = "str_foundation",
    ExpectedContainerRevision = ledger.Read("ctn_owner").Revision,
    ExpectedStructureRevision = placement.Revision("clm_home"), Kind = RemovalKind.Undo };
var undone = placement.Remove(undo);
Check(undone.MaterialDelta == 1 && Quantity(PieceKind.Foundation) == 4 &&
    ReferenceEquals(undone, placement.Remove(undo)), "full undo returns one original bundle once");
var total = kinds.Sum(Quantity);
Check(total == 10 && placement.Structures("clm_home").Count == 2,
    "material conservation across consumes and compensation");
