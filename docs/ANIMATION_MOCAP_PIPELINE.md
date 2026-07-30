# UE 5.8 animation and mocap pipeline

## Plugin audit

| Capability wanted | Name and status in UE 5.8 |
|---|---|
| Control Rig | `ControlRig` present |
| IK Rig / Retargeter | `IKRig` present |
| Full Body IK | `FullBodyIK` present, Experimental |
| Motion Warping | `MotionWarping` present |
| Animation Sharing | `AnimationSharing` present |
| Live Link | `LiveLink` present |
| Live Link Control Rig | `LiveLinkControlRig` present, Experimental |
| Take Recorder | present, inside the `Takes` plugin |
| Capture Manager | `CaptureManagerCore/Editor/Devices/App` present |
| MetaHuman Animator | descriptor present at `Engine/Plugins/MetaHuman/MetaHumanAnimator` |
| Markerless motion capture | no separate descriptor found; needs confirming in the GUI against MetaHuman entitlements and the UE 5.8 documentation |

None of these were enabled automatically, to stay out of the existing
`.uproject` changes. When a production task needs one, only that plugin is
enabled; an Experimental item is not treated as production-ready.

## Order of work

1. Save the Blender rig/animation file under `AssetWork/Blender`, with a backup.
2. On FBX/glTF export, record the metre/centimetre scale, axis, deform bones,
   bake settings and frame range.
3. After import, verify the skeleton, bone names, root, bind pose and the import
   log.
4. Build the IK Rig, IK Retargeter and retarget pose; record the root
   motion / root lock decision.
5. Wire up the Animation Sequence, Montage, Blend Space and Animation Blueprint.
6. Correct in Control Rig: foot sliding, root offset, contacts, loop seams,
   curves and notifies.
7. During mocap cleanup, check jitter, penetration, floor height and the frame
   rate conversion.
8. Video-to-mocap only through the official MetaHuman Animator tools; anything
   experimental or account-gated stays manual.

No animation is applied to a game asset during this setup, and nothing is
imported into `Content`.
