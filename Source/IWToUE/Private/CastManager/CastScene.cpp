#include "CastManager/CastScene.h"

#include "CastManager/CastAnimation.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastRoot.h"
#include "CastManager/CastNode.h"

int32 FCastScene::GetNodeCount() const
{
	return Nodes.Num();
}

int32 FCastScene::GetMaterialCount() const
{
	int32 Res = 0;
	for (const FCastRoot& Root : Roots)
	{
		Res += Root.Materials.Num();
	}
	return Res;
}

int32 FCastScene::GetTextureCount() const
{
	int32 Res = 0;
	for (const FCastRoot& Root : Roots)
	{
		for (const FCastMaterialInfo& Material : Root.Materials)
		{
			Res += Material.FileMap.Num();
		}
	}
	return Res;
}

int32 FCastScene::GetSkinnedMeshNum() const
{
	int32 Res = 0;
	for (const FCastRoot& Root : Roots)
		for (const FCastModelInfo& Model : Root.Models)
			Res += Model.Meshes[0].MaxWeight > 0;
	return Res;
}

int32 FCastScene::GetMeshNum() const
{
	int32 Res = 0;
	for (const FCastRoot& Root : Roots)
		for (const FCastModelInfo& Model : Root.Models)
			Res += Model.Meshes.Num();
	return Res;
}

bool FCastScene::HasAnimation() const
{
	for (const FCastRoot& Root : Roots)
	{
		for (const FCastAnimationInfo& Animation : Root.Animations)
		{
			if (Animation.Curves.Num() > 0)
			{
				return true;
			}
		}
	}
	return false;
}

float FCastScene::GetAnimFramerate() const
{
	for (const FCastRoot& Root : Roots)
	{
		for (const FCastAnimationInfo& Animation : Root.Animations)
		{
			return Animation.Framerate;
		}
	}
	return 0;
}

FCastNode* FCastScene::GetNode(int32 Idx)
{
	if (Nodes.IsValidIndex(Idx))
	{
		return &Nodes[Idx];
	}
	return nullptr;
}