# UE 5.8 animasyon ve mocap hattı

## Yerel plugin denetimi

| İstenen yetenek | UE 5.8 yerel adı/durum |
|---|---|
| Control Rig | `ControlRig` mevcut |
| IK Rig / Retargeter | `IKRig` mevcut |
| Full Body IK | `FullBodyIK` mevcut, Experimental |
| Motion Warping | `MotionWarping` mevcut |
| Animation Sharing | `AnimationSharing` mevcut |
| Live Link | `LiveLink` mevcut |
| Live Link Control Rig | `LiveLinkControlRig` mevcut, Experimental |
| Take Recorder | `Takes` plugin’i içinde mevcut |
| Capture Manager | `CaptureManagerCore/Editor/Devices/App` mevcut |
| MetaHuman Animator | `MetaHuman` descriptor’ı `Engine/Plugins/MetaHuman/MetaHumanAnimator` altında mevcut |
| Markerless Motion Capture | Ayrı descriptor bulunmadı; MetaHuman araç/Fab yetkisi ve UE 5.8 dokümantasyonuyla GUI’de doğrulanmalı |

Kurulum görevi mevcut `.uproject` değişikliklerine karışmamak için bu pluginleri otomatik etkinleştirmedi. Gereken üretim görevi geldiğinde yalnız ilgili plugin etkinleştirilir; Experimental öğe production-ready sayılmaz.

## Hattın kabul sırası

1. Blender rig/animation dosyasını `AssetWork/Blender` altında yedekli kaydet.
2. FBX/glTF exportta metre/santimetre ölçeği, eksen, deform bones, bake ve frame aralığını kaydet.
3. Import sonrası skeleton, bone names, root, bind pose ve import logunu doğrula.
4. IK Rig, IK Retargeter ve retarget pose oluştur; root motion/root lock kararını kaydet.
5. Animation Sequence, Montage, Blend Space ve Animation Blueprint ilişkilerini kur.
6. Control Rig düzeltmesi; foot sliding, root offset, contact, loop seam ve curve/notifies kontrolü.
7. Mocap cleanup sırasında jitter, penetrasyon, floor height ve frame rate dönüşümünü doğrula.
8. Video-to-mocap yalnız resmî MetaHuman Animator araçlarıyla; deneysel/hesap gerektiren adımlar manuel.

Bu kurulum sırasında oyun assetine animasyon uygulanmaz ve Content’e import yapılmaz.
