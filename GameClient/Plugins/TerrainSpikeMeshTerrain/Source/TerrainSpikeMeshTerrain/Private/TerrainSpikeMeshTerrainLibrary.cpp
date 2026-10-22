#include "TerrainSpikeMeshTerrainLibrary.h"

#include "Editor.h"
#include "MeshPartition.h"
#include "MeshPartitionHeightmapImporter.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, TerrainSpikeMeshTerrain)

AActor* UTerrainSpikeMeshTerrainLibrary::ImportHeightmap(const FString& PngPath, int32 ResX, int32 ResY, FVector Size, int32 SectionsX, int32 SectionsY, FString& Error)
{
    UE::MeshPartition::FHeightmapImportParams Params;
    Params.World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    Params.HeightmapFilename = PngPath;
    Params.MeshResolution = FInt32Point(ResX, ResY);
    Params.MeshSize = Size;
    Params.SectionsResolution = FInt32Point(SectionsX, SectionsY);
    Params.bSaveAndUnload = false;
    Params.LocationVolumesResolution = FInt32Point(1, 1);
    if (!Params.World) { Error = TEXT("no editor world"); return nullptr; }
    UE::MeshPartition::FHeightmapImporter Importer(Params);
    if (!Importer.Import(nullptr)) { Error = Importer.GetErrorText().ToString(); return nullptr; }
    return Importer.GetMegaMesh();
}
