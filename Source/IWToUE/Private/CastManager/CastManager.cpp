#include "CastManager/CastManager.h"

#include "CastManager/CastAnimation.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastNode.h"
#include "CastManager/CastRoot.h"
#include "Serialization/LargeMemoryReader.h"
#include "Utils/BinaryReader.h"

FCastManager::~FCastManager()
{
	Destroy();
}

bool FCastManager::Initialize(FString InFilePath)
{
	FilePath = InFilePath;
	if (!FFileHelper::LoadFileToArray(FileDataOld, *FilePath))
	{
		return false;
	}
	Reader = MakeUnique<FLargeMemoryReader>(FileDataOld.GetData(), FileDataOld.Num());

	return true;
}

bool FCastManager::Import()
{
	Scene = MakeShared<FCastScene>();
	if (!Scene) return false;

	auto& [Magic, Version, RootNodes, Flags] = Scene->Header;
	Reader->ByteOrderSerialize(&Magic, sizeof(Magic));
	*Reader << Version;
	*Reader << RootNodes;
	*Reader << Flags;

	if (Magic != 0x74736163) return false;

	Scene->RootNodes.Empty();
	for (uint32 i = 0; i < RootNodes; ++i)
	{
		Scene->RootNodes.Add(GetNode());
	}

	for (int32 Idx : Scene->RootNodes)
	{
		ProcessCastData(Scene->Nodes[Idx]);
	}

	return true;
}

void FCastManager::ReleaseScene()
{
	Scene.Reset();
}

void FCastManager::Destroy()
{
	if (bWasDestroy) return;
	bWasDestroy = true;
	if (Scene.IsValid())
	{
		Scene.Reset();
		Scene = nullptr;
	}
	if (Reader.IsValid()) Reader.Reset();
}

int32 FCastManager::GetBoneNum() const
{
	int32 Res = 0;
	for (FCastRoot& Root : Scene->Roots)
	{
		for (FCastModelInfo& Model : Root.Models)
		{
			for (FCastSkeletonInfo& Skeleton : Model.Skeletons)
			{
				Res += Skeleton.Bones.Num();
			}
		}
	}
	return Res;
}

int32 FCastManager::GetVertexNum() const
{
	int32 Res = 0;
	for (FCastRoot& Root : Scene->Roots)
	{
		for (FCastModelInfo& Model : Root.Models)
		{
			for (FCastMeshInfo& Mesh : Model.Meshes)
			{
				Res += Mesh.VertexPositions.Num();
			}
		}
	}
	return Res;
}

int32 FCastManager::GetFaceNum() const
{
	int32 Res = 0;
	for (FCastRoot& Root : Scene->Roots)
	{
		for (FCastModelInfo& Model : Root.Models)
		{
			for (FCastMeshInfo& Mesh : Model.Meshes)
			{
				Res += Mesh.Faces.Num() / 3;
			}
		}
	}
	return Res;
}

int32 FCastManager::GetNode()
{
	const int32 Idx = Scene->Nodes.Add(FCastNode());
	FCastNodeHeader& NodeHeader = Scene->Nodes[Idx].Header;
	TArray<FCastNodeProperty>& Properties = Scene->Nodes[Idx].Properties;

	*Reader << NodeHeader.Identifier;
	*Reader << NodeHeader.NodeSize;
	*Reader << NodeHeader.NodeHash;
	*Reader << NodeHeader.PropertyCount;
	*Reader << NodeHeader.ChildCount;

	for (uint32 j = 0; j < NodeHeader.PropertyCount; ++j)
	{
		FCastNodeProperty Property;
		*Reader << Property.Identifier;
		*Reader << Property.NameSize;
		*Reader << Property.ArrayLength;
		const float ArrayLength = Property.ArrayLength;

		for (uint32 k = 0; k < Property.NameSize; ++k)
		{
			ANSICHAR Tmp;
			*Reader << Tmp;
			Property.PropertyName.AppendChar(Tmp);
		}

		Property.DataType = Property.Identifier
			                    ? static_cast<ECastPropertyId>(Property.Identifier)
			                    : ECastPropertyId::String;

		switch (Property.DataType)
		{
		case ECastPropertyId::Byte:
			{
				Property.ByteArray.SetNum(ArrayLength);
				Reader->Serialize(Property.ByteArray.GetData(), Property.ArrayLength * sizeof(uint8));
				break;
			}
		case ECastPropertyId::Short:
			{
				Property.ShortArray.SetNum(ArrayLength);
				Reader->Serialize(Property.ShortArray.GetData(), Property.ArrayLength * sizeof(uint16));
				break;
			}
		case ECastPropertyId::Integer32:
			{
				Property.IntArray.SetNum(ArrayLength);
				Reader->Serialize(Property.IntArray.GetData(), ArrayLength * sizeof(int32));
				break;
			}
		case ECastPropertyId::Integer64:
			{
				Property.Int64Array.SetNum(ArrayLength);
				Reader->Serialize(Property.Int64Array.GetData(), ArrayLength * sizeof(int64));
				break;
			}
		case ECastPropertyId::Float:
			{
				Property.FloatArray.SetNum(ArrayLength);
				Reader->Serialize(Property.FloatArray.GetData(), ArrayLength * sizeof(float));
				break;
			}
		case ECastPropertyId::Double:
			{
				Property.DoubleArray.SetNum(ArrayLength);
				Reader->Serialize(Property.DoubleArray.GetData(), ArrayLength * sizeof(double));
				break;
			}
		case ECastPropertyId::String:
			{
				FBinaryReader::ReadString(*Reader, &Property.StringData);
				break;
			}
		case ECastPropertyId::Vector2:
			{
				Property.Vector2Array.SetNum(ArrayLength);
				for (FVector2f& Vector : Property.Vector2Array) *Reader << Vector;
				break;
			}
		case ECastPropertyId::Vector3:
			{
				Property.Vector3Array.SetNum(ArrayLength);
				for (FVector3f& Vector : Property.Vector3Array) *Reader << Vector;
				break;
			}
		case ECastPropertyId::Vector4:
			{
				Property.Vector4Array.SetNum(ArrayLength);
				for (FVector4f& Vector : Property.Vector4Array) *Reader << Vector;
				break;
			}
		default:
			break;
		}
		Properties.Add(Property);
	}
	const uint32 ChildNodeCount = NodeHeader.ChildCount;
	for (uint32 k = 0; k < ChildNodeCount; ++k)
	{
		if (k >= ChildNodeCount) break;
		int32 NewIdx = GetNode();
		Scene->Nodes[Idx].ChildNodes.Add(NewIdx);
	}
	return Idx;
}

void FCastManager::ProcessCastData(FCastNode& Node)
{
	switch (Node.Header.Identifier)
	{
	case 0x746F6F72: // Root
		{
			FCastRoot Root;
			for (uint32 Idx : Node.ChildNodes)
			{
				ProcessRootData(Scene->Nodes[Idx], Root);
			}
			Scene->Roots.Add(Root);
			break;
		}
	default: break;
	}
}

void FCastManager::ProcessRootData(FCastNode& Node, FCastRoot& Root)
{
	switch (Node.Header.Identifier)
	{
	case 0x6C646F6D: // Model
		{
			FCastModelInfo Model;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Model.Name = Property.PropertyName;
				}
			}
			for (uint32 Idx : Node.ChildNodes)
			{
				ProcessModelData(Scene->Nodes[Idx], Root, Model);
			}
			Root.Models.Add(Model);
			break;
		}
	case 0x6D696E61: // Animation
		{
			FCastAnimationInfo Animation;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Animation.Name = Property.PropertyName;
				}
				else if (Property.PropertyName == "f")
				{
					Animation.Framerate = Property.FloatArray[0];
				}
				else if (Property.PropertyName == "b")
				{
					Animation.Looping = static_cast<bool>(Property.ByteArray[0]);
				}
			}
			for (uint32 Idx : Node.ChildNodes)
			{
				ProcessAnimationData(Scene->Nodes[Idx], Animation);
			}
			Root.Animations.Add(Animation);
			break;
		}
	case 0x74736E69: // Instance
		{
			FCastInstanceInfo Instance;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Instance.Name = Property.StringData;
				}
				else if (Property.PropertyName == "rf")
				{
					Instance.ReferenceFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "p")
				{
					Instance.Position = Property.Vector3Array[0];
				}
				else if (Property.PropertyName == "r")
				{
					Instance.Rotation = Property.Vector4Array[0];
				}
				else if (Property.PropertyName == "s")
				{
					Instance.Scale = Property.Vector3Array[0];
				}
			}
			Root.Instances.Add(Instance);
			break;
		}
	case 0x6174656D: // Metadata
		{
			FCastMetadataInfo Metadata;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "p")
				{
					Metadata.Author = Property.StringData;
				}
				else if (Property.PropertyName == "s")
				{
					Metadata.Software = Property.StringData;
				}
				if (Property.PropertyName == "up")
				{
					Metadata.UpAxis = Property.StringData;
				}
			}
			Root.Metadata.Add(Metadata);
			break;
		}
	default: break;
	}
}

void FCastManager::ProcessModelData(FCastNode& Node, FCastRoot& Root, FCastModelInfo& Model)
{
	switch (Node.Header.Identifier)
	{
	case 0x6873656D: // Mesh
		{
			FCastMeshInfo Mesh;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Mesh.Name = Property.StringData;
				}
				else if (Property.PropertyName == "vp")
				{
					Mesh.VertexPositions = Property.Vector3Array;
				}
				else if (Property.PropertyName == "vn")
				{
					Mesh.VertexNormals = Property.Vector3Array;
				}
				else if (Property.PropertyName == "vt")
				{
					Mesh.VertexTangents = Property.Vector3Array;
				}
				else if (Property.PropertyName == "wb")
				{
					Mesh.VertexWeightBone.Empty();
					if (Property.DataType == ECastPropertyId::Integer32)
						COPY_ARR(Property.IntArray, Mesh.VertexWeightBone)
					if (Property.DataType == ECastPropertyId::Short)
						COPY_ARR(Property.ShortArray, Mesh.VertexWeightBone)
					if (Property.DataType == ECastPropertyId::Byte)
						COPY_ARR(Property.ByteArray, Mesh.VertexWeightBone)
				}
				else if (Property.PropertyName == "wv")
				{
					Mesh.VertexWeightValue = Property.FloatArray;
				}
				else if (Property.PropertyName == "f")
				{
					Mesh.Faces.Empty();
					if (Property.DataType == ECastPropertyId::Integer32)
						COPY_ARR(Property.IntArray, Mesh.Faces)
					if (Property.DataType == ECastPropertyId::Short)
						COPY_ARR(Property.ShortArray, Mesh.Faces)
					if (Property.DataType == ECastPropertyId::Byte)
						COPY_ARR(Property.ByteArray, Mesh.Faces)
				}
				else if (Property.PropertyName == "cl")
				{
					Mesh.ColorLayer.Empty();
					if (Property.DataType == ECastPropertyId::Integer32)
						COPY_ARR(Property.IntArray, Mesh.ColorLayer)
					if (Property.DataType == ECastPropertyId::Short)
						COPY_ARR(Property.ShortArray, Mesh.ColorLayer)
					if (Property.DataType == ECastPropertyId::Byte)
						COPY_ARR(Property.ByteArray, Mesh.ColorLayer)
				}
				else if (Property.PropertyName == "ul")
				{
					if (Property.DataType == ECastPropertyId::Integer32)
						Mesh.UVLayer = Property.IntArray[0];
					if (Property.DataType == ECastPropertyId::Short)
						Mesh.UVLayer = Property.ShortArray[0];
					if (Property.DataType == ECastPropertyId::Byte)
						Mesh.UVLayer = Property.ByteArray[0];
				}
				else if (Property.PropertyName == "mi")
				{
					if (Property.DataType == ECastPropertyId::Integer32)
						Mesh.MaxWeight = Property.IntArray[0];
					if (Property.DataType == ECastPropertyId::Short)
						Mesh.MaxWeight = Property.ShortArray[0];
					if (Property.DataType == ECastPropertyId::Byte)
						Mesh.MaxWeight = Property.ByteArray[0];
				}
				else if (Property.PropertyName == "sm")
				{
					Mesh.SkinningMethod = Property.StringData;
				}
				else if (Property.PropertyName == "m")
				{
					Mesh.MaterialHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "vc")
				{
					// if (Mesh.VertexColor.IsEmpty()) Mesh.VertexColor.Add(Property.IntArray);
					Mesh.VertexColor = Property.IntArray;
				}
				else if (Property.PropertyName[0] == 'c')
				{
					// int32 ArrIdx = Property.PropertyName[1] - '0';
					// if (Mesh.VertexColor.Num() <= ArrIdx) Mesh.VertexColor.SetNum(ArrIdx + 1);
					// Mesh.VertexColor[ArrIdx] = Property.IntArray;
					Mesh.VertexColor = Property.IntArray;
				}
				else if (Property.PropertyName[0] == 'u')
				{
					int32 ArrIdx = Property.PropertyName[1] - '0';
					if (Mesh.VertexUVs.Num() <= ArrIdx) Mesh.VertexUVs.SetNum(ArrIdx + 1);
					Mesh.VertexUVs[ArrIdx] = Property.Vector2Array;
				}
			}
			Model.Meshes.Add(Mesh);
		}
	case 0x68736C62: // BlendShape
		{
			FCastBlendShape BlendShape;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					BlendShape.Name = Property.StringData;
				}
				else if (Property.PropertyName == "b")
				{
					BlendShape.MeshHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "vi")
				{
					BlendShape.TargetShapeVertexIndices.Empty();
					if (Property.DataType == ECastPropertyId::Integer32)
						COPY_ARR(Property.IntArray, BlendShape.TargetShapeVertexIndices)
					if (Property.DataType == ECastPropertyId::Short)
						COPY_ARR(Property.ShortArray, BlendShape.TargetShapeVertexIndices)
					if (Property.DataType == ECastPropertyId::Byte)
						COPY_ARR(Property.ByteArray, BlendShape.TargetShapeVertexIndices)
				}
				else if (Property.PropertyName == "vp")
				{
					BlendShape.TargetShapeVertexPositions = Property.Vector3Array;
				}
				else if (Property.PropertyName == "ts")
				{
					BlendShape.TargetWeightScale = Property.FloatArray;
				}
			}
			Model.BlendShapes.Add(BlendShape);
			break;
		}
	case 0x6C656B73: // Skeleton
		{
			FCastSkeletonInfo Skeleton;
			for (uint32 Idx : Node.ChildNodes)
			{
				ProcessSkeletonData(Scene->Nodes[Idx], Skeleton);
			}
			Model.Skeletons.Add(Skeleton);
			break;
		}
	case 0x6C74616D: // Material
		{
			FCastMaterialInfo Material;
			Material.MaterialHash = Node.Header.NodeHash;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Material.Name = Property.StringData;
				}
				else if (Property.PropertyName == "t")
				{
					Material.Type = Property.StringData;
				}
				else if (Property.PropertyName == "albedo")
				{
					Material.AlbedoFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "diffuse")
				{
					Material.DiffuseFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "normal")
				{
					Material.NormalFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "specular")
				{
					Material.SpecularFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "emissive")
				{
					Material.EmissiveFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "gloss")
				{
					Material.GlossFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "roughness")
				{
					Material.RoughnessFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "ao")
				{
					Material.AmbientOcclusionFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "cavity")
				{
					Material.CavityFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "aniso")
				{
					Material.AnisotropyFileHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "extra%d")
				{
					Material.ExtraFileHash = Property.Int64Array[0];
				}
			}
			for (uint32 Idx : Node.ChildNodes)
			{
				ProcessMaterialData(Scene->Nodes[Idx], Material);
			}
			int32 MatIdx = Root.Materials.Add(Material);
			Root.MaterialMap.Add(Material.MaterialHash, MatIdx);
			break;
		}
	}
}

void FCastManager::ProcessSkeletonData(FCastNode& Node, FCastSkeletonInfo& Skeleton)
{
	switch (Node.Header.Identifier)
	{
	case 0x656E6F62: // Bone
		{
			FCastBoneInfo Bone;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Bone.BoneName = Property.StringData;
				}
				else if (Property.PropertyName == "p")
				{
					Bone.ParentIndex = Property.IntArray[0];
				}
				else if (Property.PropertyName == "ssc")
				{
					Bone.SegmentScaleCompensate = static_cast<bool>(Property.ByteArray[0]);
				}
				else if (Property.PropertyName == "lp")
				{
					Bone.LocalPosition = Property.Vector3Array[0];
				}
				else if (Property.PropertyName == "lr")
				{
					Bone.LocalRotation = Property.Vector4Array[0];
				}
				else if (Property.PropertyName == "wp")
				{
					Bone.WorldPosition = Property.Vector3Array[0];
				}
				else if (Property.PropertyName == "wr")
				{
					Bone.WorldRotation = Property.Vector4Array[0];
				}
				else if (Property.PropertyName == "s")
				{
					Bone.Scale = Property.Vector3Array[0];
				}
			}
			Skeleton.Bones.Add(Bone);
			break;
		}
	case 0x64686B69: // IKHandle
		{
			FCastIKHandle IKHandle;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					IKHandle.Name = Property.StringData;
				}
				else if (Property.PropertyName == "n")
				{
					IKHandle.StartBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "n")
				{
					IKHandle.EndBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "n")
				{
					IKHandle.TargetBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "n")
				{
					IKHandle.PoleVectorBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "n")
				{
					IKHandle.PoleBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "n")
				{
					IKHandle.UseTargetRotation = static_cast<bool>(Property.ByteArray[0]);
				}
			}
			Skeleton.IKHandles.Add(IKHandle);
			break;
		}
	case 0x74736E63: // Constraint
		{
			FCastConstraint Constraint;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
				{
					Constraint.Name = Property.StringData;
				}
				else if (Property.PropertyName == "ct")
				{
					Constraint.ConstraintType = Property.StringData;
				}
				else if (Property.PropertyName == "cb")
				{
					Constraint.ConstraintBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "tb")
				{
					Constraint.TargetBoneHash = Property.Int64Array[0];
				}
				else if (Property.PropertyName == "mo")
				{
					Constraint.MaintainOffset = static_cast<bool>(Property.ByteArray[0]);
				}
				else if (Property.PropertyName == "sx")
				{
					Constraint.SkipX = static_cast<bool>(Property.ByteArray[0]);
				}
				else if (Property.PropertyName == "sy")
				{
					Constraint.SkipY = static_cast<bool>(Property.ByteArray[0]);
				}
				else if (Property.PropertyName == "sz")
				{
					Constraint.SkipZ = static_cast<bool>(Property.ByteArray[0]);
				}
			}
			Skeleton.Constraints.Add(Constraint);
			break;
		}
	}
}

void FCastManager::ProcessMaterialData(FCastNode& Node, FCastMaterialInfo& Material)
{
	for (auto Property : Node.Properties)
		if (Property.PropertyName == "p")
			Material.FileMap.Add(Node.Header.NodeHash, Property.StringData);
}

void FCastManager::ProcessAnimationData(FCastNode& Node, FCastAnimationInfo& Animation)
{
	switch (Node.Header.Identifier)
	{
	case 0x76727563: // Curve
		{
			FCastCurveInfo Curve;
			for (FCastNodeProperty& Property : Node.Properties)
			{
				if (Property.PropertyName == "nn")
				{
					Curve.NodeName = Property.StringData;
				}
				else if (Property.PropertyName == "kp")
				{
					Curve.KeyPropertyName = Property.StringData;
				}
				else if (Property.PropertyName == "kb")
				{
					if (Property.DataType == ECastPropertyId::Integer32)
						COPY_ARR(Property.IntArray, Curve.KeyFrameBuffer)
					if (Property.DataType == ECastPropertyId::Short)
						COPY_ARR(Property.ShortArray, Curve.KeyFrameBuffer)
					if (Property.DataType == ECastPropertyId::Byte)
						COPY_ARR(Property.ByteArray, Curve.KeyFrameBuffer)
				}
				else if (Property.PropertyName == "kv")
				{
					Curve.KeyValueBuffer.Empty();
					if (Property.DataType == ECastPropertyId::Byte)
						for (const auto& Val : Property.ByteArray)
							Curve.KeyValueBuffer.Add(Val);
					if (Property.DataType == ECastPropertyId::Short)
						for (const auto& Val : Property.ShortArray)
							Curve.KeyValueBuffer.Add(Val);
					if (Property.DataType == ECastPropertyId::Integer32)
						for (const auto& Val : Property.IntArray)
							Curve.KeyValueBuffer.Add(Val);
					if (Property.DataType == ECastPropertyId::Float)
						for (const auto& Val : Property.FloatArray)
							Curve.KeyValueBuffer.Add(Val);
					if (Property.DataType == ECastPropertyId::Vector4)
						for (const auto& Val : Property.Vector4Array)
							Curve.KeyValueBuffer.Add(FVector4(Val));
				}
				else if (Property.PropertyName == "m")
				{
					Curve.Mode = Property.StringData;
				}
				else if (Property.PropertyName == "ab")
				{
					Curve.AdditiveBlendWeight = Property.FloatArray[0];
				}
			}
			Animation.Curves.Add(Curve);
			break;
		}
	case 0x564F4D43: // CurveModeOverride
		{
			FCastCurveModeOverrideInfo CurveModeOverride;
			for (auto Property : Node.Properties)
			{
				if (Property.PropertyName == "nn")
				{
					CurveModeOverride.NodeName = Property.StringData;
				}
				else if (Property.PropertyName == "m")
				{
					CurveModeOverride.Mode = Property.StringData;
				}
				else if (Property.PropertyName == "ot")
				{
					CurveModeOverride.OverrideTranslationCurves = static_cast<bool>(Property.ByteArray[0]);
				}
				else if (Property.PropertyName == "or")
				{
					CurveModeOverride.OverrideRotationCurves = static_cast<bool>(Property.ByteArray[0]);
				}
				else if (Property.PropertyName == "os")
				{
					CurveModeOverride.OverrideScaleCurves = static_cast<bool>(Property.ByteArray[0]);
				}
			}
			Animation.CurveModeOverrides.Add(CurveModeOverride);
			break;
		}
	case 0x6669746E: // NotificationTrack
		{
			FCastNotificationTrackInfo NotificationTrack;
			NotificationTrack.KeyFrameBuffer.Empty();
			for (FCastNodeProperty& Property : Node.Properties)
			{
				if (Property.PropertyName == "n")
					NotificationTrack.Name = Property.StringData;
				else if (Property.PropertyName == "kb")
					if (Property.DataType == ECastPropertyId::Integer32)
						for (const auto& Val : Property.IntArray)
							NotificationTrack.KeyFrameBuffer.Add(Val);
				// COPY_ARR(Property.IntArray, NotificationTrack.KeyFrameBuffer)
				if (Property.DataType == ECastPropertyId::Short)
					// for (const auto& Val : src_arr) dst_arr.Add(Val)
					COPY_ARR(Property.ShortArray, NotificationTrack.KeyFrameBuffer)
				if (Property.DataType == ECastPropertyId::Byte)
					COPY_ARR(Property.ByteArray, NotificationTrack.KeyFrameBuffer)
			}
			Animation.NotificationTracks.Add(NotificationTrack);
			break;
		}
	}
}
