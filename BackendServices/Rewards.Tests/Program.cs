using ShatteredFrontier.InventoryLedger;
using ShatteredFrontier.Rewards;

if (args.Length != 1) throw new ArgumentException("Pass the loot content directory.");
var table = LootCatalog.LoadFiles(Directory.EnumerateFiles(args[0], "*.json"))["loot/greenlands_silverbrook_boar"];
BoarRewardService.ValidateBoarTable(table);
var store = new InMemoryInventoryLedgerStore();
var ledger = new ShatteredFrontier.InventoryLedger.InventoryLedger(store);
ledger.CreateContainer(new ContainerSpec("ctn_boar", OwnerKind.Character, "chr_killer", ContainerKind.Backpack, 100));
ledger.CreateContainer(new ContainerSpec("ctn_partner", OwnerKind.Character, "chr_partner", ContainerKind.Backpack, 100));
var rewards = new BoarRewardService(ledger, table);
var defeat = new DefeatEvent("defeat-1", "chr_killer", "chr_killer", "ctn_boar", 0, false, false);
var first = rewards.Grant(defeat);
if (first.Decision != RewardDecision.Granted || first.SourceCode != "loot_drop" || first.Receipt?.OperationId != "defeat:8:defeat-1:chr_killer")
    throw new Exception("Expected one sourced reward receipt.");
var amount = ledger.Read("ctn_boar").Quantities["item/boar_hide"];
if (amount is < 2 or > 3 || rewards.Grant(defeat).Receipt != first.Receipt ||
    ledger.Read("ctn_boar").Quantities["item/boar_hide"] != amount)
    throw new Exception("Retry changed the reward.");
var partner = defeat with { RecipientCharacterId = "chr_partner", ContainerId = "ctn_partner" };
var partnerGrant = rewards.Grant(partner);
if (partnerGrant.Decision != RewardDecision.Granted ||
    ledger.Read("ctn_partner").Quantities["item/boar_hide"] != partnerGrant.Receipt!.Changes[0].Delta ||
    rewards.Grant(partner).Receipt != partnerGrant.Receipt)
    throw new Exception("Shared kill did not make a separate personal grant.");
var secondService = new BoarRewardService(ledger, table);
if (secondService.Grant(defeat).Receipt?.OperationId != first.Receipt.OperationId ||
    ledger.Read("ctn_boar").Quantities["item/boar_hide"] != amount ||
    secondService.Grant(partner).Receipt?.OperationId != partnerGrant.Receipt.OperationId)
    throw new Exception("Ledger replay changed the reward.");
if (BoarRewardService.Roll(defeat.Id + ":" + defeat.RecipientCharacterId, table) != amount) throw new Exception("Roll is not deterministic.");
var denied = new DefeatEvent("defeat-2", "chr_killer", "chr_killer", "ctn_boar", 1, true, false);
if (rewards.Grant(denied).Decision != RewardDecision.Ineligible || rewards.Grant(denied).Receipt != null ||
    ledger.Read("ctn_boar").Revision != 1)
    throw new Exception("Protected town reward was issued.");
try { rewards.Grant(denied with { KillerInProtectedTown = false }); throw new Exception("Changed retry accepted."); }
catch (InvalidOperationException ex) when (ex.Message.Contains("Conflicting defeat")) { }
var noCredit = denied with { Id = "defeat-3", KillerCharacterId = "", KillerInProtectedTown = false, RecipientCharacterId = "chr_other" };
if (rewards.Grant(noCredit).Decision != RewardDecision.Ineligible) throw new Exception("Uncredited kill was rewarded.");
try
{
    LootCatalog.Validate(table with { Entries = [new LootEntry("item", "item/boar_hide", new CountRange(1, 1), 0)] });
    throw new Exception("Zero weight accepted.");
}
catch (RewardContentException) { }
Console.WriteLine("Rewards tests passed: roll, receipt replay, eligibility, validation.");
