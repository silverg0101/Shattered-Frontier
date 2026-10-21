using ShatteredFrontier.TerrainDelta;
using ShatteredFrontier.TerrainDelta.Postgres;

static void Check(bool value, string message) { if (!value) throw new Exception(message); }
static void Reject<T>(Action action) where T : Exception
{
    try { action(); throw new Exception($"Expected {typeof(T).Name}."); }
    catch (T) { }
}
var memory = new InMemoryTerrainDeltaStore();
var region = new RegionKey("zone_1", -3, 7);
var edit = new TerrainEdit("op_one",region,"char_a","claim_a",new(-12,4,9),TerrainKind.Dig,100);
Check(memory.Apply(edit,0).Revision == 1,"memory apply");
Check(memory.Apply(edit,0).Revision == 1,"memory replay");
Reject<TerrainConflictException>(() => memory.Apply(edit with { Magnitude = 101 },1));
Reject<TerrainConflictException>(() => memory.Apply(edit with { OperationId = "op_two" },0));
Check(memory.Compensate("undo_one","op_one",region,"char_a","claim_a",1).Revision == 2,"memory tombstone");
Reject<TerrainConflictException>(() => memory.Compensate("undo_two","op_one",region,"char_a","claim_a",2));
Check(memory.ListRegion(region,1).Entries.Single().Receipt.Tombstone,"memory tail");
Reject<ArgumentOutOfRangeException>(() => memory.Apply(edit with { Magnitude = 32768 },2));
Console.WriteLine("PASS model");

var connection = Environment.GetEnvironmentVariable("TERRAIN_DELTA_CONNECTION") ?? throw new Exception("Missing connection.");
var store = new PostgresTerrainDeltaStore(connection);
var mode = args.SingleOrDefault() ?? "seed";
if (mode == "seed")
{
    Check(store.Apply(edit,0).Revision == 1,"apply");
    Check(store.Apply(edit,0).Revision == 1,"idempotent replay");
    Reject<TerrainConflictException>(() => store.Apply(edit with { Magnitude = 101 },1));
    Reject<TerrainConflictException>(() => store.Apply(edit with { OperationId = "stale" },0));
    Check(store.ListRegion(region).Entries.Count == 1,"list");
    Console.WriteLine("PASS durable seed/replay/CAS");
}
else if (mode == "restart")
{
    Check(store.Apply(edit,0).Revision == 1,"restart replay");
    Reject<TerrainConflictException>(() => store.Compensate("wrong","op_one",region,"char_b","claim_a",1));
    Check(store.Compensate("undo_one","op_one",region,"char_a","claim_a",1).Revision == 2,"compensate");
    Check(store.Compensate("undo_one","op_one",region,"char_a","claim_a",1).Revision == 2,"compensation replay");
    Reject<TerrainConflictException>(() => store.Compensate("undo_two","op_one",region,"char_a","claim_a",2));
    var listing = store.ListRegion(region,1);
    Check(listing.Revision == 2 && listing.Entries.Count == 1 && listing.Entries[0].Receipt.Tombstone,"tail tombstone");
    Console.WriteLine("PASS restart/authority/tombstone/tail");
}
else if (mode == "schema")
{
    Reject<TerrainSchemaCompatibilityException>(() => store.ListRegion(region));
    Console.WriteLine("PASS schema refusal");
}
else throw new Exception("Unknown fixture mode.");
