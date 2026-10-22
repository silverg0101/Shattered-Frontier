if (args.Length != 1) { Console.Error.WriteLine("Usage: Exchange.Tool <content/markets directory>"); return 2; }
try
{
    _ = MarketCatalog.Load(args[0]);
    Console.WriteLine("MARKET_CONTENT_OK");
    return 0;
}
catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }
