using System.Reflection;
using System.Text.Json;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;

// Dependency loading only: no connections, listeners or gameplay contracts.
var value = new StringValue { Value = "dependency-probe" };
if (StringValue.Parser.ParseFrom(value.ToByteArray()).Value != value.Value)
    throw new InvalidOperationException("Protobuf runtime roundtrip failed.");
string[] assemblyNames = ["Grpc.AspNetCore.Server", "Grpc.Net.Client", "Google.Protobuf", "Npgsql",
    "Confluent.Kafka", "OpenTelemetry.Extensions.Hosting", "OpenTelemetry.Instrumentation.AspNetCore",
    "OpenTelemetry.Exporter.OpenTelemetryProtocol"];
var assemblies = assemblyNames.Select(name => Assembly.Load(name).GetName())
    .ToDictionary(name => name.Name!, name => name.Version!.ToString());
Console.WriteLine(JsonSerializer.Serialize(new { runtime = Environment.Version.ToString(),
    kafkaNative = Confluent.Kafka.Library.VersionString, protobufRoundtrip = true, assemblies }));
