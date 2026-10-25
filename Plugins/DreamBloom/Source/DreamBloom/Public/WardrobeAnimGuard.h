#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformAtomics.h"
#include "EngineLogs.h"
#include "BonePose.h"

/**
 * Helpers that stop the DreamBloom anim nodes from walking a broken compact pose parent chain.
 *
 * FCSPose<FCompactPose>::CalculateComponentSpaceTransform resolves a bone by walking
 * Pose.GetParentBoneIndex(), which reads FBoneContainer::CompactPoseParentBones directly:
 *
 *     const BoneIndexType ParentIndex = Pose.GetParentBoneIndex(BoneIndex);
 *     if (ComponentSpaceFlags[ParentIndex] == 0)      // no range check on the lookup itself
 *
 * When that table hands back an index outside [0, Pose.GetNumBones()) - a stale FBoneContainer
 * after an LOD switch, or a parent that was not mapped while the container was rebuilt - the very
 * next line trips TArray::RangeCheck and drops the debugger into a breakpoint *inside the parallel
 * animation evaluation*, taking the whole anim graph down with it. That is the 0x80000003 crash
 * that shows up at a different bone (and in a different node) every run: it is whoever walks the
 * chain first after the layout went bad, not a fault in that particular node.
 *
 * IsValidIndex() alone does not cover this: the bone itself is in range, it is the *parent* that
 * is not. Validating the chain before transforming lets a node skip its own contribution for one
 * frame instead of killing the process.
 */
namespace WardrobeAnimGuard
{
	/**
	 * Walks the compact pose parent chain of Bone.
	 * Returns false when the chain leaves the pose, loops, or is deeper than the pose has bones.
	 * Safe to call: every index is range checked before it is used to look up its parent.
	 */
	inline bool IsParentChainValid(const FCompactPose& Pose, const FCompactPoseBoneIndex& Bone)
	{
		const int32 NumBones = Pose.GetNumBones();
		if (!Bone.IsValid() || Bone.GetInt() >= NumBones)
		{
			return false;
		}

		FCompactPoseBoneIndex Current = Bone;
		for (int32 Depth = 0; Depth <= NumBones; ++Depth)
		{
			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Current);
			if (!Parent.IsValid())
			{
				// Reached the root: the chain is well formed.
				return true;
			}
			if (Parent.GetInt() >= NumBones)
			{
				return false;
			}
			Current = Parent;
		}

		// Walked further than the pose has bones: the parent table contains a cycle.
		return false;
	}

	/** Bone is addressable and every parent on its chain is too. */
	inline bool IsUsable(const FCompactPose& Pose, const FCompactPoseBoneIndex& Bone)
	{
		return Pose.IsValidIndex(Bone) && IsParentChainValid(Pose, Bone);
	}

	/** Rate limited so a persistent fault cannot flood the log from the worker threads. */
	inline void ReportBrokenChain(const TCHAR* NodeName, const FCompactPoseBoneIndex& Bone, int32 NumBones)
	{
		static int32 ReportCount = 0;
		const int32 Count = FPlatformAtomics::InterlockedIncrement(&ReportCount);
		if (Count <= 8 || (Count % 600) == 0)
		{
			UE_LOG(LogAnimation, Warning,
			       TEXT("%s: compact pose parent chain is broken at bone %d (pose bones=%d); skipped this frame. ")
			       TEXT("The FBoneContainer was most likely rebuilt (LOD or mesh change) while this pose still ")
			       TEXT("referenced the previous layout."),
			       NodeName, Bone.GetInt(), NumBones);
		}
	}
}
