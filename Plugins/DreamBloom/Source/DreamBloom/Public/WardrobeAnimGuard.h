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
 * When that table hands back an index outside [0, Pose.GetNumBones()) - a parent that was not
 * mapped while the container was rebuilt, or a parent filtered out of the required bones - the
 * very next line trips TArray::RangeCheck and drops the debugger into a breakpoint *inside the
 * parallel animation evaluation*, taking the whole anim graph down with it. That is the
 * 0x80000003 crash that shows up at a different bone (and in a different node) every run: it is
 * whoever walks the chain first after the layout went bad, not a fault in that particular node.
 *
 * IsValidIndex() alone does not cover this: the bone itself is in range, it is the *parent* that
 * is not. Validating the chain before transforming lets a node skip its own contribution for one
 * frame instead of killing the process.
 *
 * HOW A PARENT GOES MISSING
 * FBoneContainer::Initialize() writes INDEX_NONE into CompactPoseParentBones whenever a bone's
 * parent is not part of the pose's required bones. FCSPose::InitPose() then marks only compact
 * index 0 as evaluated (ComponentSpaceFlags[0] = 1), i.e. the engine assumes a compact pose has
 * exactly one root. Such a bone is a *second root*: asking for its component space transform
 * reaches CalculateComponentSpaceTransform with ParentIndex == -1 and crashes on the first
 * ComponentSpaceFlags[ParentIndex].
 *
 * THE HOLE THIS FILE USED TO HAVE
 * The previous version ended the walk at the first bone without a parent and returned true
 * ("reached the root: the chain is well formed"). A second root is indistinguishable from the
 * real root by the parent table alone, so the guard let through exactly the case it was written
 * for: FAnimNode_WardrobePinch called the base implementation, which asserted in
 * AnimNode_ModifyBone.cpp:53 -> BonePose.h:624. Only compact index 0 may end the walk; any other
 * bone whose parent is unmapped is a broken chain.
 */
namespace WardrobeAnimGuard
{
	/**
	 * Walks the compact pose parent chain of Bone.
	 *
	 * @return the compact index whose parent is missing from this pose, or INDEX_NONE when the walk
	 *         reaches compact root 0 without leaving the pose. Safe to call: every index is range
	 *         checked before it is used to look up its parent.
	 */
	inline int32 FindBrokenChainIndex(const FCompactPose& Pose, const FCompactPoseBoneIndex& Bone)
	{
		const int32 NumBones = Pose.GetNumBones();
		FCompactPoseBoneIndex Current = Bone;

		for (int32 Depth = 0; Depth <= NumBones; ++Depth)
		{
			if (!Current.IsValid() || Current.GetInt() >= NumBones)
			{
				// The start index itself is not addressable in this pose.
				return Bone.GetInt();
			}

			// Compact root. FCSPose pre-marks it as evaluated and never calculates it, so both the
			// engine and this guard are safe to stop here.
			if (Current.GetInt() == 0)
			{
				return INDEX_NONE;
			}

			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Current);
			if (!Parent.IsValid() || Parent.GetInt() >= NumBones)
			{
				// The parent is not in this pose's required bones. FCSPose would index
				// ComponentSpaceFlags[-1] here, so treat the chain as broken.
				return Current.GetInt();
			}

			Current = Parent;
		}

		// Walked further than the pose has bones: the parent table contains a cycle.
		return Bone.GetInt();
	}

	/** Bone is addressable and every parent on its chain leads back to compact root 0. */
	inline bool IsParentChainValid(const FCompactPose& Pose, const FCompactPoseBoneIndex& Bone)
	{
		return FindBrokenChainIndex(Pose, Bone) == INDEX_NONE;
	}

	/** Bone is addressable and every parent on its chain is too. */
	inline bool IsUsable(const FCompactPose& Pose, const FCompactPoseBoneIndex& Bone)
	{
		return Pose.IsValidIndex(Bone) && IsParentChainValid(Pose, Bone);
	}

	/** compact index -> bone name, for logging only. NAME_None when the index is out of range. */
	inline FName GetBoneName(const FCompactPose& Pose, const int32 CompactIndex)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (CompactIndex < 0 || CompactIndex >= Bones.GetCompactPoseNumBones())
		{
			return NAME_None;
		}

		const int32 SkeletonIndex = Bones.GetSkeletonIndex(FCompactPoseBoneIndex(CompactIndex));
		const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
		return (SkeletonIndex >= 0 && SkeletonIndex < RefSkeleton.GetNum())
			       ? RefSkeleton.GetBoneName(SkeletonIndex)
			       : NAME_None;
	}

	/** Rate limited so a persistent fault cannot flood the log from the worker threads. */
	inline void ReportBrokenChain(const TCHAR* NodeName, const FCompactPose& Pose, const FCompactPoseBoneIndex& Bone)
	{
		static int32 ReportCount = 0;
		const int32 Count = FPlatformAtomics::InterlockedIncrement(&ReportCount);
		if (Count <= 8 || (Count % 600) == 0)
		{
			const int32 BrokenIndex = FindBrokenChainIndex(Pose, Bone);
			UE_LOG(LogAnimation, Warning,
			       TEXT("%s: compact pose parent chain of bone %d (%s) is broken at bone %d (%s), pose bones=%d; ")
			       TEXT("skipped this frame. The parent of the broken bone is missing from this pose's required bones ")
			       TEXT("(mesh bone exclusion, LOD bone filter, or a container rebuilt under a live pose), so FCSPose ")
			       TEXT("would read ComponentSpaceFlags[-1] and assert. Add that parent chain to the required bones ")
			       TEXT("to get the effect back."),
			       NodeName, Bone.GetInt(), *GetBoneName(Pose, Bone.GetInt()).ToString(), BrokenIndex,
			       *GetBoneName(Pose, BrokenIndex).ToString(), Pose.GetNumBones());
		}
	}
}
