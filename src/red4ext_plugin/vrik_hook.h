#pragma once
#include "vrik_solver.h"
#include <MinHook.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <windows.h>


#pragma pack(push, 1)
struct UniversalTrackingData {
  uint32_t version;
  uint32_t updateCounter;
  struct Pose {
    float pos[3];
    float rot[4];
    float trigger;
    float grip;
    float stickX;
    float stickY;
    uint64_t buttons;
    uint32_t valid;
    uint32_t padding;
  };
  Pose head;
  Pose leftHand;
  Pose rightHand;
};
#pragma pack(pop)

extern RED4ext::Vector4 g_CameraWorldPos;
extern int g_CalibrationBoneIndex;
extern void* g_PlayerAnimComponent;
extern volatile uint64_t g_hookTotalCalls;
extern volatile uint64_t g_hookMatchCalls;
extern volatile uint64_t g_hookBoneWrites;
extern volatile int g_hookCapture;
extern volatile uint64_t g_hookSkeletalCalls;
extern volatile int g_capturedCount;
extern volatile uint32_t g_capturedRcx[32];
extern volatile uint64_t g_capturedFull[32];

// VR bone variables
extern int16_t g_VRBoneParent[256];
extern volatile int g_VRBoneCount;
extern volatile int g_VRRightUpperArmIdx;
extern volatile int g_VRRightForeArmIdx;
extern volatile int g_VRLeftUpperArmIdx;
extern volatile int g_VRLeftForeArmIdx;
extern volatile int g_VRRightShoulderIdx;
extern volatile int g_VRLeftShoulderIdx;
extern volatile int g_VRBind;
extern volatile float g_VRIKShoulderWidthScale;

// Shoulder rigid lock state
static bool g_VRIKShoulderCaptured = false;
static float g_VRIKShoulderOffsetR[3] = {0};
static float g_VRIKShoulderOffsetL[3] = {0};
static float g_VRIKShoulderRotR[4] = {0,0,0,1};
static float g_VRIKShoulderRotL[4] = {0,0,0,1};
static float g_VRIKShoulderClavOffsetR[3] = {0};
static float g_VRIKShoulderClavOffsetL[3] = {0};

typedef void *(*ComponentFunc_t)(void *rcx, void *rdx, void *r8, void *r9);
static ComponentFunc_t OriginalFunc21 = nullptr;

// Page-readable guard: returns true only if [p, p+n) is committed and readable.
// Needed because the capture path dereferences arbitrary component pointers;
// __try alone is unreliable here, so we pre-validate the page protection.
static inline bool VRIK_IsReadable(const void *p, size_t n) {
  if (!p)
    return false;
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0)
    return false;
  if (mbi.State != MEM_COMMIT)
    return false;
  DWORD prot = mbi.Protect & 0xFF;
  if (prot == PAGE_NOACCESS || prot == 0)
    return false;
  if (mbi.Protect & PAGE_GUARD)
    return false;
  uintptr_t start = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
  uintptr_t end = start + mbi.RegionSize;
  uintptr_t a = reinterpret_cast<uintptr_t>(p);
  return (a + n) <= end;
}

extern "C" void *Hooked_ComponentFunc21(void *rcx, void *rdx, void *r8,
                                        void *r9) {
  // 1. Call original function first (let the AnimGraph calculate the pose)
  void *result = OriginalFunc21(rcx, rdx, r8, r9);

  ++g_hookTotalCalls;

  // Capture mode: find which rcx pointers have a skeleton flowing through this
  // function. Every deref is pre-validated with VirtualQuery (page must be
  // committed+readable) because this runs on EVERY component, including
  // non-animated ones where +0x168 is not a pointer. __try is kept only as a
  // last-resort backstop.
  if (g_hookCapture && VRIK_IsReadable(rcx, 0x170)) {
    __try {
      void *mi =
          *reinterpret_cast<void **>(reinterpret_cast<uint8_t *>(rcx) + 0x168);
      if (VRIK_IsReadable(mi, 0xE8)) {
        void *ba =
            *reinterpret_cast<void **>(reinterpret_cast<uint8_t *>(mi) + 0xE0);
        if (VRIK_IsReadable(ba, 0x20)) {
          ++g_hookSkeletalCalls;
          uint32_t low = static_cast<uint32_t>(
              reinterpret_cast<uintptr_t>(rcx) & 0xFFFFFFFF);
          int n = g_capturedCount;
          bool seen = false;
          for (int i = 0; i < n && i < 32; ++i) {
            if (g_capturedRcx[i] == low) {
              seen = true;
              break;
            }
          }
          if (!seen && n < 32) {
            g_capturedRcx[n] = low;
            g_capturedFull[n] = reinterpret_cast<uintptr_t>(rcx);
            g_capturedCount = n + 1;
          }
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
  }

  // 2. Filter for Player's AnimatedComponent
  if (rcx && g_PlayerAnimComponent && rcx == g_PlayerAnimComponent) {
    ++g_hookMatchCalls;
    __try {
      // Read ModelInstance
      void **modelInstancePtr =
          reinterpret_cast<void **>(reinterpret_cast<uint8_t *>(rcx) + 0x168);
      if (modelInstancePtr && *modelInstancePtr) {

        // Read Bone Array at +0xE0
        void **boneArrayPtr = reinterpret_cast<void **>(
            reinterpret_cast<uint8_t *>(*modelInstancePtr) + 0xE0);
        if (boneArrayPtr && *boneArrayPtr) {
          uint8_t *bones = reinterpret_cast<uint8_t *>(*boneArrayPtr);
          ++g_hookBoneWrites;

          if (g_CalibrationBoneIndex == -2) {
            // Test: shove bone 0 (root) +2.0m on every axis - large +
            // multi-axis so it is visible even in first person, on whichever
            // component is correct.
            reinterpret_cast<float *>(bones + 0)[0] += 2.0f;
            reinterpret_cast<float *>(bones + 4)[0] += 2.0f;
            reinterpret_cast<float *>(bones + 8)[0] += 2.0f;
          } else if (g_CalibrationBoneIndex >= 0 &&
                     g_CalibrationBoneIndex < 150) {
            // 32 bytes per QsTransform. Offset 8 is Z position (local).
            float *z = reinterpret_cast<float *>(
                bones + (g_CalibrationBoneIndex * 32) + 8);
            *z += 0.5f; // Stretch!

            float *x = reinterpret_cast<float *>(
                bones + (g_CalibrationBoneIndex * 32) + 0);
            *x += 0.5f;
          }
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      // Safe read failed
    }
  }

  return result;
}

// ---------------------------------------------------------------------------
// Pose-apply hook on sub_14017DDB4 (RVA 0x17DDB4), found via Cheat Engine
// write-BP on the player's live track buffer. This function copies the
// evaluated pose into the destination skeleton:
//   a2[7][0] = bone transform buffer (48 bytes/bone == QsTransform:
//              Translation@+0, Rotation(x,y,z,w)@+16, Scale@+32 --
//              authoritative from RED4ext generated QsTransform.hpp; the old
//              comment had it inverted, which is what made the floating hand
//              misbehave)
//   a2[7][3] = track value buffer (used to identify the player)
// The buffer holds PARENT-LOCAL transforms, so a hand can't be placed by
// writing its translation directly -- VRIK_SolveArm does model-space FK +
// 2-bone IK and writes only LOCAL ROTATIONS (no translation writes => no skin
// stretch). We run AFTER the original, so writing hand bones here survives
// graph eval.
extern volatile uintptr_t g_PlayerTrackBufA;
extern volatile uintptr_t g_PlayerTrackBufB;
extern volatile int
    g_AnimPoseDebug; // 1 = push bones 0..63; 2 = push single test bone
extern volatile uint64_t g_AnimPoseTotalCalls;
extern volatile uint64_t g_AnimPoseMatchCalls;
extern volatile uintptr_t
    g_AnimPoseLastBoneBuf; // last matched player bone buffer (debug)
extern volatile int g_AnimPoseTestBone;  // single-bone test index (mode 2)
extern volatile float g_AnimPoseTestMag; // single-bone test magnitude (mode 2)
extern volatile uintptr_t g_PlayerPuppetPtr; // Pointer to PlayerPuppet instance

// VR hand binding: write VR controller pose into the hand bones each frame.
extern UniversalTrackingData *g_pBodyWalkTracking;
extern volatile int
    g_VRBind; // 0 off, 1=right pos, 2=right pos+rot, 3=both pos(+rot)
extern volatile float g_VRBindScale; // position scale (VR units -> model units)
extern volatile float g_VRBindOffX;
extern volatile float g_VRBindOffY;
extern volatile float g_VRBindOffZ;
extern volatile int g_VRBindAxis; // axis-remap preset 0..5
// Constant per-hand wrist-orientation correction (hand-local), set live via
// SetVRHandOffset(pitch,yaw,roll,hand). Applied as handRot = mapQuat *
// wristCorr. Defaults are the calibrated values: right = euler(0,-90,0), left =
// euler(-180,-90,0).
extern volatile float g_VRWristR_I, g_VRWristR_J, g_VRWristR_K, g_VRWristR_R;
extern volatile float g_VRWristL_I, g_VRWristL_J, g_VRWristL_K, g_VRWristL_R;
// Per-hand reach scale + position offset (mode 4). Different arm
// lengths/heights per user.
extern volatile float g_VRScaleR, g_VRScaleL;
extern volatile float g_VROffRX, g_VROffRY, g_VROffRZ;
extern volatile float g_VROffLX, g_VROffLY, g_VROffLZ;
// Per-hand elbow pole spin (degrees): fine rotation of the bend normal around
// the shoulder->hand axis, to nudge the elbow more outward/inward. 0 = natural.
extern volatile float g_VRElbowPoleR, g_VRElbowPoleL;
extern volatile float g_VRElbowSwingR, g_VRElbowSwingL;
extern volatile int g_VRRightBoneIdx; // default 24 (RightHand)
extern volatile int g_VRLeftBoneIdx;  // default 23 (LeftHand)
extern volatile int g_VRHeadBoneIdx;  // head bone (resolved by name), -1 = none
extern volatile int
    g_VRUseHeadRelative; // 1 = compose hand pose relative to the head bone
extern volatile int
    g_VRDiagCapture; // 1 = snapshot bones 0..31 (pre-write) into g_VRDiagBones
extern float g_VRDiagBones[32 * 7]; // per bone: translation(3) + quaternion(4),
                                    // in buffer space

extern volatile float g_VRPlayerYaw; // player world yaw (degrees), pushed from Lua each frame
extern volatile float g_VRCamI, g_VRCamJ, g_VRCamK, g_VRCamR; // FPP camera (HMD) world quaternion
extern volatile float g_VRYawCompensation; // multiplier for relative yaw

// IK diagnostics (last solve, model space) -- surfaced via LogVRDiag.
extern volatile float g_VRIKDbgTarget[3];
extern volatile float g_VRIKDbgShoulder[3];
extern volatile float g_VRIKDbgElbow[3];
extern volatile float g_VRIKDbgLens[2]; // upperArmLen, foreArmLen
extern volatile float
    g_VRIKDbgLocal[4]; // hand pos in body frame: lx,ly,lz, crossAmount
extern volatile float g_VRIKDbgTargetL[3]; // same, LEFT arm
extern volatile float g_VRIKDbgShoulderL[3];
extern volatile float g_VRIKDbgElbowL[3];
extern volatile float g_VRIKDbgLensL[2];
extern volatile float g_VRIKDbgLocalL[4];

// Maps a VR-space vector to model-space per the selected preset (VR is Y-up).
static inline void VRIK_RemapAxis(int preset, const float *v, float *o) {
  switch (preset) {
  default:
  case 0:
    o[0] = v[0];
    o[1] = v[1];
    o[2] = v[2];
    break; // identity
  case 1:
    o[0] = v[0];
    o[1] = -v[2];
    o[2] = v[1];
    break; // Y-up -> Z-up (Standard OpenXR)
  case 2:
    o[0] = v[0];
    o[1] = v[2];
    o[2] = v[1];
    break;
  case 3:
    o[0] = -v[0];
    o[1] = -v[2];
    o[2] = v[1];
    break;
  case 4:
    o[0] = v[2];
    o[1] = v[0];
    o[2] = v[1];
    break;
  case 5:
    o[0] = -v[2];
    o[1] = -v[0];
    o[2] = v[1];
    break;
  }
}

// Rotate vector v by quaternion q (q = i,j,k,r == x,y,z,w). o = q * v * q^-1.
static inline void VRIK_QuatRotateVec(const float *q, const float *v,
                                      float *o) {
  const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
  const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
  const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
  o[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
  o[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
  o[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

// Hamilton product o = a * b (both i,j,k,r == x,y,z,w).
static inline void VRIK_QuatMul(const float *a, const float *b, float *o) {
  o[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  o[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  o[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  o[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

// Conjugate (== inverse for a unit quaternion).
static inline void VRIK_QuatConj(const float *q, float *o) {
  o[0] = -q[0];
  o[1] = -q[1];
  o[2] = -q[2];
  o[3] = q[3];
}

static inline void VRIK_QuatNorm(float *q) {
  float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (n > 1e-8f) {
    float inv = 1.0f / n;
    q[0] *= inv;
    q[1] *= inv;
    q[2] *= inv;
    q[3] *= inv;
  } else {
    q[0] = 0.0f;
    q[1] = 0.0f;
    q[2] = 0.0f;
    q[3] = 1.0f;
  }
}

static inline float VRIK_Dot3(const float *a, const float *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static inline void VRIK_Cross3(const float *a, const float *b, float *o) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}
static inline float VRIK_Norm3(float *v) {
  float n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (n > 1e-8f) {
    float inv = 1.0f / n;
    v[0] *= inv;
    v[1] *= inv;
    v[2] *= inv;
  }
  return n;
}

// Shortest-arc unit quaternion rotating unit vector a onto unit vector b.
static inline void VRIK_QuatFromTo(const float *a, const float *b, float *o) {
  float d = VRIK_Dot3(a, b);
  if (d >= 1.0f - 1e-6f) {
    o[0] = 0.0f;
    o[1] = 0.0f;
    o[2] = 0.0f;
    o[3] = 1.0f;
    return;
  }
  if (d <= -1.0f + 1e-6f) {
    // Antiparallel: rotate 180 deg about any axis orthogonal to a.
    float ax[3] = {1.0f, 0.0f, 0.0f};
    if (std::fabs(a[0]) > 0.9f) {
      ax[0] = 0.0f;
      ax[1] = 1.0f;
      ax[2] = 0.0f;
    }
    float axis[3];
    VRIK_Cross3(a, ax, axis);
    VRIK_Norm3(axis);
    o[0] = axis[0];
    o[1] = axis[1];
    o[2] = axis[2];
    o[3] = 0.0f;
    return;
  }
  float c[3];
  VRIK_Cross3(a, b, c);
  o[0] = c[0];
  o[1] = c[1];
  o[2] = c[2];
  o[3] = 1.0f + d;
  VRIK_QuatNorm(o);
}

// Writes one VR hand into the destination bone buffer (48-byte QsTransform:
// rotation/quaternion @ +0, translation @ +16, scale @ +32 -- confirmed via IDA
// decompile of sub_14017DDB4). When head-relative is active the controller's
// head-local offset is rotated by the head bone's orientation and added to the
// head bone's position, so the result lands in the same buffer space as the
// head. This mirrors the working CET gizmo (worldPos = camPos + camQuat *
// localPos) and is what stops the hand from swinging when the head turns.
static inline void VRIK_WriteHand(uint8_t *boneBuf, int bIdx,
                                  const float *headPos, const float *headQuat,
                                  bool headOk, const float *vrPos,
                                  const float *vrQuat, bool writeRot) {
  if (bIdx < 0)
    return;

  const float s = g_VRBindScale;
  float local[3];
  VRIK_RemapAxis(g_VRBindAxis, vrPos, local);
  local[0] *= s;
  local[1] *= s;
  local[2] *= s;

  float pos[3];
  if (g_VRUseHeadRelative && headOk) {
    float rotated[3];
    VRIK_QuatRotateVec(headQuat, local, rotated);
    pos[0] = headPos[0] + rotated[0];
    pos[1] = headPos[1] + rotated[1];
    pos[2] = headPos[2] + rotated[2];
  } else {
    pos[0] = local[0];
    pos[1] = local[1];
    pos[2] = local[2];
  }
  pos[0] += g_VRBindOffX;
  pos[1] += g_VRBindOffY;
  pos[2] += g_VRBindOffZ;

  // Translation @ +0 (QsTransform), Rotation @ +16 -- see file header.
  float *t = reinterpret_cast<float *>(boneBuf + bIdx * 48 + 0);
  t[0] = pos[0];
  t[1] = pos[1];
  t[2] = pos[2];

  if (writeRot) {
    // VR->game axis swap, same as the gizmo's mapLocalQuat: (i, -k, j, r).
    float localQuat[4] = {vrQuat[0], -vrQuat[2], vrQuat[1], vrQuat[3]};
    float *r = reinterpret_cast<float *>(boneBuf + bIdx * 48 + 16);
    if (g_VRUseHeadRelative && headOk) {
      VRIK_QuatMul(headQuat, localQuat, r);
    } else {
      r[0] = localQuat[0];
      r[1] = localQuat[1];
      r[2] = localQuat[2];
      r[3] = localQuat[3];
    }
  }
}

// QsTransform field offsets inside each 48-byte bone slot.
static constexpr int VRIK_TRANS_OFF = 0; // Translation (Vector4)
static constexpr int VRIK_ROT_OFF = 16;  // Rotation (Quaternion x,y,z,w)

// Model-space FK scratch (recomputed each matched frame). Sized generously.
static constexpr int VRIK_MAX_BONES = 256;
static float g_fkPos[VRIK_MAX_BONES][3];
static float g_fkRot[VRIK_MAX_BONES][4];

// Forward kinematics: accumulate parent-local transforms into model space.
// Requires parent index < child index (true for these rigs / topological
// order).
static inline void VRIK_ComputeFK(uint8_t *boneBuf, int count) {
  if (count > VRIK_MAX_BONES)
    count = VRIK_MAX_BONES;
  for (int i = 0; i < count; ++i) {
    const float *lt =
        reinterpret_cast<float *>(boneBuf + i * 48 + VRIK_TRANS_OFF);
    const float *lr =
        reinterpret_cast<float *>(boneBuf + i * 48 + VRIK_ROT_OFF);
    float lpos[3] = {lt[0], lt[1], lt[2]};
    float lrot[4] = {lr[0], lr[1], lr[2], lr[3]};
    int p = g_VRBoneParent[i];
    if (p >= 0 && p < i) {
      VRIK_QuatMul(g_fkRot[p], lrot, g_fkRot[i]);
      VRIK_QuatNorm(g_fkRot[i]);
      float rp[3];
      VRIK_QuatRotateVec(g_fkRot[p], lpos, rp);
      g_fkPos[i][0] = g_fkPos[p][0] + rp[0];
      g_fkPos[i][1] = g_fkPos[p][1] + rp[1];
      g_fkPos[i][2] = g_fkPos[p][2] + rp[2];
    } else {
      g_fkRot[i][0] = lrot[0];
      g_fkRot[i][1] = lrot[1];
      g_fkRot[i][2] = lrot[2];
      g_fkRot[i][3] = lrot[3];
      VRIK_QuatNorm(g_fkRot[i]);
      g_fkPos[i][0] = lpos[0];
      g_fkPos[i][1] = lpos[1];
      g_fkPos[i][2] = lpos[2];
    }
  }
}

// Writes a model-space rotation back into a bone as a LOCAL rotation, given the
// (already updated) model rotation of its parent. localRot = parentModel^-1 *
// modelRot.
static inline void VRIK_WriteLocalRot(uint8_t *boneBuf, int idx,
                                      const float *parentModelRot,
                                      const float *modelRot) {
  float pInv[4];
  VRIK_QuatConj(parentModelRot, pInv);
  float local[4];
  VRIK_QuatMul(pInv, modelRot, local);
  VRIK_QuatNorm(local);
  float *r = reinterpret_cast<float *>(boneBuf + idx * 48 + VRIK_ROT_OFF);
  r[0] = local[0];
  r[1] = local[1];
  r[2] = local[2];
  r[3] = local[3];
}

// Two-bone arm IK in model space. Rotates upper arm + forearm so the wrist
// reaches targetModel, and orients the hand to handModelRot. Only rotations are
// written; translations (bone lengths) are untouched, so the mesh never
// stretches.
static inline void VRIK_SolveArm(uint8_t *boneBuf, int upperIdx, int foreIdx,
                                 int handIdx, const float *targetModel,
                                 const float *handModelRot,
                                 const float *bodyRight, const float *bodyUp,
                                 const float *bodyFwd, float poleAngleRad,
                                 float swingGain, bool isLeft, bool storeDbg) {
  if (upperIdx < 0 || foreIdx < 0 || handIdx < 0)
    return;
  if (upperIdx >= VRIK_MAX_BONES || foreIdx >= VRIK_MAX_BONES ||
      handIdx >= VRIK_MAX_BONES)
    return;

  const float *sh = g_fkPos[upperIdx];
  const float *el = g_fkPos[foreIdx];
  const float *wr = g_fkPos[handIdx];

  float curUp[3] = {el[0] - sh[0], el[1] - sh[1], el[2] - sh[2]};
  float curFore[3] = {wr[0] - el[0], wr[1] - el[1], wr[2] - el[2]};
  float upLen = VRIK_Norm3(curUp);
  float foreLen = VRIK_Norm3(curFore);
  if (upLen < 1e-4f || foreLen < 1e-4f)
    return;

  // Direction and clamped distance shoulder -> target.
  float toTarget[3] = {targetModel[0] - sh[0], targetModel[1] - sh[1],
                       targetModel[2] - sh[2]};
  float dist = VRIK_Norm3(toTarget);
  float minReach = std::fabs(upLen - foreLen) + 1e-3f;
  float maxReach = upLen + foreLen - 1e-3f;
  if (dist < minReach)
    dist = minReach;
  if (dist > maxReach)
    dist = maxReach;

  // Elbow swivel -- TRANSPARENT, anatomy-driven model (replaces the opaque
  // VRArmIK polynomial, which read as a near-constant spin in this rig's body
  // frame, so the slider just SHIFTED the elbow in space instead of letting it
  // FOLLOW the hand's arc).
  //
  // The elbow bend direction 'up0' (which way the elbow drops off the straight
  // shoulder->hand line) is built from body axes and is non-degenerate
  // everywhere:
  //   base = down(-bodyUp) + back(-bodyFwd), each projected perpendicular to
  //   the arm axis.
  //     A pure "down" pole degenerates in the bicep-curl pose (hand at face ->
  //     axis points UP
  //     -> down anti-parallel -> forearm can't stand vertical). "back" is
  //     perpendicular there and vice-versa for the forward arm, so the sum is
  //     stable. swing==0 == this base == the clean vertical curl (already
  //     CONFIRMED good).
  //   + OUTWARD swing: when the hand reaches ACROSS the body (crosses to its
  //   opposite side) the
  //     real elbow swings OUT to its own side. crossAmount = max(0,
  //     sideways*sideSign) grows ONLY while crossing and adds an own-side
  //     outward component scaled by swingGain. THIS is what makes the elbow "go
  //     to the side" as the hand sweeps its arc, while leaving the curl and
  //     same-side reaches untouched.
  float armLen = upLen + foreLen;
  float rel[3] = {targetModel[0] - sh[0], targetModel[1] - sh[1],
                  targetModel[2] - sh[2]};
  float sideSign = isLeft ? 1.0f : -1.0f;
  float lx =
      (rel[0] * bodyRight[0] + rel[1] * bodyRight[1] + rel[2] * bodyRight[2]) /
      armLen; // + = avatar right
  float ly = (rel[0] * bodyUp[0] + rel[1] * bodyUp[1] + rel[2] * bodyUp[2]) /
             armLen; // + = up
  float lz = (rel[0] * bodyFwd[0] + rel[1] * bodyFwd[1] + rel[2] * bodyFwd[2]) /
             armLen; // + = forward
  float crossAmount = lx * sideSign;
  if (crossAmount < 0.0f)
    crossAmount = 0.0f; // hand crossing body

  float axis[3] = {toTarget[0], toTarget[1],
                   toTarget[2]}; // shoulder->hand, unit
  float downRef[3] = {-bodyUp[0], -bodyUp[1], -bodyUp[2]};
  float backRef[3] = {-bodyFwd[0], -bodyFwd[1], -bodyFwd[2]};
  float outRef[3] = {-sideSign * bodyRight[0], -sideSign * bodyRight[1],
                     -sideSign * bodyRight[2]}; // own side
  float dDot =
      downRef[0] * axis[0] + downRef[1] * axis[1] + downRef[2] * axis[2];
  float bDot =
      backRef[0] * axis[0] + backRef[1] * axis[1] + backRef[2] * axis[2];
  float oDot = outRef[0] * axis[0] + outRef[1] * axis[1] + outRef[2] * axis[2];
  float dPerp[3] = {downRef[0] - axis[0] * dDot, downRef[1] - axis[1] * dDot,
                    downRef[2] - axis[2] * dDot};
  float bPerp[3] = {backRef[0] - axis[0] * bDot, backRef[1] - axis[1] * bDot,
                    backRef[2] - axis[2] * bDot};
  float oPerp[3] = {outRef[0] - axis[0] * oDot, outRef[1] - axis[1] * oDot,
                    outRef[2] - axis[2] * oDot};
  // Base bend direction (curl + forward poses -- CONFIRMED good): down+back
  // blend, normalized.
  float up0[3] = {dPerp[0] + bPerp[0], dPerp[1] + bPerp[1],
                  dPerp[2] + bPerp[2]};
  if (VRIK_Norm3(up0) < 1e-3f) {
    up0[0] = oPerp[0];
    up0[1] = oPerp[1];
    up0[2] = oPerp[2];
    if (VRIK_Norm3(up0) < 1e-3f) {
      up0[0] = 1.0f;
      up0[1] = 0.0f;
      up0[2] = 0.0f;
    }
  }
  // Outward swing: blend the bend direction TOWARD the own-side outward dir by
  // a fraction f, gated by BOTH crossing and BEND. Diag (both hands at chest)
  // was decisive: a plain forward reach reads cross~=0.20 while the LEFT hand
  // at chest read only cross=0.138 -- so a cross deadzone can't separate them
  // (0.20 killed the left). The real distinguisher is DISTANCE: a forward reach
  // is STRAIGHT (dist ~= armLen) while a hand at the chest is BENT (dist ~=
  // 0.5*armLen). So bendFactor enables the swing only when the arm is bent
  // (forward/curl-straight poses -> 0 -> untouched), and crossGate ramps it in
  // once the hand crosses past a tiny margin. Blend of UNIT directions (the old
  // additive sum cancelled in X to ~0.12 = "elbow down").
  if (VRIK_Norm3(oPerp) > 1e-4f) {
    float normDist = dist / armLen; // 1=straight, ~0.5=bent at chest
    float bendFactor = (0.90f - normDist) *
                       (1.0f / 0.35f); // 0 at straight, 1 by normDist 0.55
    if (bendFactor < 0.0f)
      bendFactor = 0.0f;
    if (bendFactor > 1.0f)
      bendFactor = 1.0f;
    float crossGate = (crossAmount - 0.05f) *
                      (1.0f / 0.10f); // soft margin 0.05, full by 0.15
    if (crossGate < 0.0f)
      crossGate = 0.0f;
    if (crossGate > 1.0f)
      crossGate = 1.0f;
    float f = swingGain * crossGate * bendFactor;
    if (f < 0.0f)
      f = 0.0f;
    if (f > 1.0f)
      f = 1.0f;
    up0[0] = (1.0f - f) * up0[0] + f * oPerp[0];
    up0[1] = (1.0f - f) * up0[1] + f * oPerp[1];
    up0[2] = (1.0f - f) * up0[2] + f * oPerp[2];
    if (VRIK_Norm3(up0) < 1e-3f) {
      up0[0] = oPerp[0];
      up0[1] = oPerp[1];
      up0[2] = oPerp[2];
    }
  }
  if (storeDbg) {
    volatile float *L = isLeft ? g_VRIKDbgLocalL : g_VRIKDbgLocal;
    L[0] = lx;
    L[1] = ly;
    L[2] = lz;
    L[3] = crossAmount;
  }
  // poleAngleRad: constant fine spin of the bend direction around the arm axis
  // (Elbow pole).
  float swivelRad = poleAngleRad;
  // Rodrigues rotation of up0 around axis by swivelRad (axis . up0 ~= 0, perp).
  float up[3];
  {
    float c = std::cos(swivelRad), s = std::sin(swivelRad);
    float cr[3];
    VRIK_Cross3(axis, up0, cr);
    float ad = axis[0] * up0[0] + axis[1] * up0[1] + axis[2] * up0[2];
    up[0] = up0[0] * c + cr[0] * s + axis[0] * ad * (1.0f - c);
    up[1] = up0[1] * c + cr[1] * s + axis[1] * ad * (1.0f - c);
    up[2] = up0[2] * c + cr[2] * s + axis[2] * ad * (1.0f - c);
    VRIK_Norm3(up);
  }

  // Law of cosines: angle at the shoulder between toTarget and the upper arm.
  float cosA =
      (upLen * upLen + dist * dist - foreLen * foreLen) / (2.0f * upLen * dist);
  if (cosA < -1.0f)
    cosA = -1.0f;
  if (cosA > 1.0f)
    cosA = 1.0f;
  float sinA = std::sqrt(1.0f - cosA * cosA);

  float newElbow[3] = {
      sh[0] + upLen * (cosA * toTarget[0] + sinA * up[0]),
      sh[1] + upLen * (cosA * toTarget[1] + sinA * up[1]),
      sh[2] + upLen * (cosA * toTarget[2] + sinA * up[2]),
  };

  // Upper arm: rotate current dir -> new elbow dir.
  float desUp[3] = {newElbow[0] - sh[0], newElbow[1] - sh[1],
                    newElbow[2] - sh[2]};
  VRIK_Norm3(desUp);
  float delta1[4];
  VRIK_QuatFromTo(curUp, desUp, delta1);
  float newUpModel[4];
  VRIK_QuatMul(delta1, g_fkRot[upperIdx], newUpModel);
  VRIK_QuatNorm(newUpModel);

  int upParent = g_VRBoneParent[upperIdx];
  const float *upParentModel = (upParent >= 0 && upParent < VRIK_MAX_BONES)
                                   ? g_fkRot[upParent]
                                   : nullptr;
  float identity[4] = {0, 0, 0, 1};
  VRIK_WriteLocalRot(boneBuf, upperIdx,
                     upParentModel ? upParentModel : identity, newUpModel);

  // Forearm: after the upper-arm delta, its base dir is rotate(delta1,
  // curFore).
  float foreBase[3];
  VRIK_QuatRotateVec(delta1, curFore, foreBase);
  float desFore[3] = {targetModel[0] - newElbow[0],
                      targetModel[1] - newElbow[1],
                      targetModel[2] - newElbow[2]};
  VRIK_Norm3(desFore);
  float delta2[4];
  VRIK_QuatFromTo(foreBase, desFore, delta2);
  float tmp[4];
  VRIK_QuatMul(delta2, delta1, tmp);
  float newForeModel[4];
  VRIK_QuatMul(tmp, g_fkRot[foreIdx], newForeModel);
  VRIK_QuatNorm(newForeModel);
  VRIK_WriteLocalRot(boneBuf, foreIdx, newUpModel, newForeModel);

  // Hand: orient to the controller (model space), written local to the new
  // forearm.
  VRIK_WriteLocalRot(boneBuf, handIdx, newForeModel, handModelRot);

  if (storeDbg) {
    volatile float *T = isLeft ? g_VRIKDbgTargetL : g_VRIKDbgTarget;
    volatile float *S = isLeft ? g_VRIKDbgShoulderL : g_VRIKDbgShoulder;
    volatile float *E = isLeft ? g_VRIKDbgElbowL : g_VRIKDbgElbow;
    volatile float *N = isLeft ? g_VRIKDbgLensL : g_VRIKDbgLens;
    T[0] = targetModel[0];
    T[1] = targetModel[1];
    T[2] = targetModel[2];
    S[0] = sh[0];
    S[1] = sh[1];
    S[2] = sh[2];
    E[0] = newElbow[0];
    E[1] = newElbow[1];
    E[2] = newElbow[2];
    N[0] = upLen;
    N[1] = foreLen;
  }
}

// Builds the model-space hand target + orientation from the VR controller.
//
// KEY: the controller position/orientation in shared memory are stored in
// HMD-LOCAL space (the producer does headInv*(hand-head)). To make head motion
// cancel out we first undo the HMD rotation by multiplying with hmdRel =
// baseInv*Qhead (shared slots 16..19): hmdRel * rawHandLocal =
// baseInv*(hand-head)world, a head-INDEPENDENT offset in the body-forward
// frame. Then the usual VR->game axis swap, anchored at the head bone's model
// position. Because the body yaw is the same in camQuat and playerYaw they
// cancel, so no extra world->model rotation is needed here (the model anchor
// handles it).
//
// hmdRel == nullptr or a zero quat falls back to identity (no head
// compensation).
static inline void VRIK_BuildHandTarget(const float *headModelPos,
                                        const float *hmdRel, const float *vrPos,
                                        const float *vrQuat,
                                        const float *wristCorr, float scale,
                                        const float *off, float *outTarget,
                                        float *outHandRot) {
  const float s = scale;

  // Undo the HMD-local frame so head turns/tilts don't drag the hand.
  float basePos[3];
  float baseQuat[4];
  bool haveHmd =
      hmdRel && (hmdRel[0] * hmdRel[0] + hmdRel[1] * hmdRel[1] +
                 hmdRel[2] * hmdRel[2] + hmdRel[3] * hmdRel[3]) > 1e-4f;
  if (haveHmd) {
    VRIK_QuatRotateVec(hmdRel, vrPos, basePos);
    VRIK_QuatMul(hmdRel, vrQuat, baseQuat);
    VRIK_QuatNorm(baseQuat);
  } else {
    basePos[0] = vrPos[0];
    basePos[1] = vrPos[1];
    basePos[2] = vrPos[2];
    baseQuat[0] = vrQuat[0];
    baseQuat[1] = vrQuat[1];
    baseQuat[2] = vrQuat[2];
    baseQuat[3] = vrQuat[3];
  }

  // VR axis -> game bone axis convention (same mapping as the proven gizmo):
  // (x,-z,y).
  float mapLocal[3] = {basePos[0] * s, -basePos[2] * s, basePos[1] * s};
  outTarget[0] = headModelPos[0] + mapLocal[0] + off[0];
  outTarget[1] = headModelPos[1] + mapLocal[1] + off[1];
  outTarget[2] = headModelPos[2] + mapLocal[2] + off[2];

  // Hand orientation: same axis swap (i,-k,j,r) on the head-independent
  // controller quat, then the per-hand constant wrist correction
  // (SetVRHandOffset) to align palm/fingers.
  float mapQuat[4] = {baseQuat[0], -baseQuat[2], baseQuat[1], baseQuat[3]};
  VRIK_QuatMul(mapQuat, wristCorr, outHandRot);
  VRIK_QuatNorm(outHandRot);
}

typedef void *(*AnimPoseFunc_t)(void *a1, void *a2, void *a3, unsigned int a4);
static AnimPoseFunc_t OriginalAnimPose = nullptr;

extern "C" inline void *Hooked_AnimPoseApply(void *a1, void *a2, void *a3,
                                             unsigned int a4) {
  void *result = OriginalAnimPose(a1, a2, a3, a4);
  ++g_AnimPoseTotalCalls;

  // Hot-path early-out: this hook runs on EVERY skeleton's pose apply (all
  // NPCs, every frame). Do nothing unless the player is armed AND we actually
  // have work. No VirtualQuery here -- that syscall per call was the FPS
  // killer. a2 is always a valid pose-apply argument, so a single __try guards
  // the dereferences.

  // -------------------------------------------------------------
  // OPTION B: OVERRIDE CROSSHAIR DATA IN TARGETING SYSTEM
  // -------------------------------------------------------------
  if (g_PlayerPuppetPtr) {
    __try {
      uintptr_t targetSys = *(uintptr_t *)(g_PlayerPuppetPtr + 0x30CC8);
      if (targetSys) {
        // +0x350 is Vector4 Position, +0x370 is Vector4 Forward
        float *pos = reinterpret_cast<float *>(targetSys + 0x350);
        float *fwd = reinterpret_cast<float *>(targetSys + 0x370);

        // TEST: Shoot straight UP (Z = 1.0) and move origin 10 meters up
        // to see if bullets go up into the sky from 10m above
        pos[2] += 10.0f; // Z += 10

        fwd[0] = 0.0f; // X
        fwd[1] = 0.0f; // Y
        fwd[2] = 1.0f; // Z (UP)
        fwd[3] = 0.0f; // W
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
  }
  // -------------------------------------------------------------

  if (!(g_PlayerTrackBufA || g_PlayerTrackBufB))
    return result;
  if (g_VRBind <= 0 && g_AnimPoseDebug == 0 && g_VRDiagCapture == 0) {
    g_VRIKShoulderCaptured = false; // Reset capture when exiting VR pose mode
    return result;
  }

  __try {
    void *poseDesc = reinterpret_cast<void **>(a2)[7];
    if (poseDesc) {
      uint8_t *boneBuf = reinterpret_cast<uint8_t **>(poseDesc)[0];
      uintptr_t trackBuf = reinterpret_cast<uintptr_t *>(poseDesc)[3];
      if (boneBuf && trackBuf &&
          (trackBuf == g_PlayerTrackBufA || trackBuf == g_PlayerTrackBufB)) {
        ++g_AnimPoseMatchCalls;
        g_AnimPoseLastBoneBuf = reinterpret_cast<uintptr_t>(boneBuf);

        // Diagnostic snapshot of the ORIGINAL (pre-write) pose of bones 0..31.
        // Correct QsTransform layout: translation@+0, rotation@+16.
        if (g_VRDiagCapture) {
          for (int b = 0; b < 32; ++b) {
            const float *t =
                reinterpret_cast<float *>(boneBuf + b * 48 + VRIK_TRANS_OFF);
            const float *q =
                reinterpret_cast<float *>(boneBuf + b * 48 + VRIK_ROT_OFF);
            g_VRDiagBones[b * 7 + 0] = t[0];
            g_VRDiagBones[b * 7 + 1] = t[1];
            g_VRDiagBones[b * 7 + 2] = t[2];
            g_VRDiagBones[b * 7 + 3] = q[0];
            g_VRDiagBones[b * 7 + 4] = q[1];
            g_VRDiagBones[b * 7 + 5] = q[2];
            g_VRDiagBones[b * 7 + 6] = q[3];
          }
        }

        if (g_AnimPoseDebug == 1) {
          // Calibration: shove translation (offset +0 in each 48-byte bone)
          // of bones 0..63 by +1.5m. Whole upper body visibly distorts.
          for (int b = 0; b < 64; ++b) {
            float *t =
                reinterpret_cast<float *>(boneBuf + b * 48 + VRIK_TRANS_OFF);
            t[0] += 1.5f;
            t[1] += 1.5f;
            t[2] += 1.5f;
          }
        } else if (g_AnimPoseDebug == 2) {
          int idx = g_AnimPoseTestBone;
          if (idx >= 0) {
            float m = g_AnimPoseTestMag;
            // +0 = translation (QsTransform: translation@0, rotation@16,
            // scale@32).
            float *t =
                reinterpret_cast<float *>(boneBuf + idx * 48 + VRIK_TRANS_OFF);
            t[0] += m;
            t[1] += m;
            t[2] += m;
          }
        }

        // VRIK FULL-ARM IK (mode 4): model-space FK + 2-bone IK, rotation-only
        // writes (no stretch). Anchored at the head bone's model position; the
        // controller offset is taken straight from the proven gizmo world math.
        if (g_VRBind == 4 && g_pBodyWalkTracking && g_VRBoneCount > 0 &&
            g_VRHeadBoneIdx >= 0) {
            
          // --- Shoulder Freeze and Scale ---
          if (g_VRRightShoulderIdx >= 0 && g_VRLeftShoulderIdx >= 0) {
              float* rotR = reinterpret_cast<float*>(boneBuf + g_VRRightShoulderIdx * 48 + VRIK_ROT_OFF);
              float* rotL = reinterpret_cast<float*>(boneBuf + g_VRLeftShoulderIdx * 48 + VRIK_ROT_OFF);
              
              if (!g_VRIKShoulderCaptured) {
                  g_VRIKShoulderRotR[0] = rotR[0]; g_VRIKShoulderRotR[1] = rotR[1]; g_VRIKShoulderRotR[2] = rotR[2]; g_VRIKShoulderRotR[3] = rotR[3];
                  g_VRIKShoulderRotL[0] = rotL[0]; g_VRIKShoulderRotL[1] = rotL[1]; g_VRIKShoulderRotL[2] = rotL[2]; g_VRIKShoulderRotL[3] = rotL[3];
                  
                  if (g_VRRightUpperArmIdx >= 0 && g_VRLeftUpperArmIdx >= 0) {
                      float* tR = reinterpret_cast<float*>(boneBuf + g_VRRightUpperArmIdx * 48 + VRIK_TRANS_OFF);
                      float* tL = reinterpret_cast<float*>(boneBuf + g_VRLeftUpperArmIdx * 48 + VRIK_TRANS_OFF);
                      g_VRIKShoulderOffsetR[0] = tR[0]; g_VRIKShoulderOffsetR[1] = tR[1]; g_VRIKShoulderOffsetR[2] = tR[2];
                      g_VRIKShoulderOffsetL[0] = tL[0]; g_VRIKShoulderOffsetL[1] = tL[1]; g_VRIKShoulderOffsetL[2] = tL[2];
                  }
                  
                  // Capture initial translation of the clavicles (Shoulders)
                  float* tClavR = reinterpret_cast<float*>(boneBuf + g_VRRightShoulderIdx * 48 + VRIK_TRANS_OFF);
                  float* tClavL = reinterpret_cast<float*>(boneBuf + g_VRLeftShoulderIdx * 48 + VRIK_TRANS_OFF);
                  g_VRIKShoulderClavOffsetR[0] = tClavR[0]; g_VRIKShoulderClavOffsetR[1] = tClavR[1]; g_VRIKShoulderClavOffsetR[2] = tClavR[2];
                  g_VRIKShoulderClavOffsetL[0] = tClavL[0]; g_VRIKShoulderClavOffsetL[1] = tClavL[1]; g_VRIKShoulderClavOffsetL[2] = tClavL[2];

                  g_VRIKShoulderCaptured = true;
              }

              // 1. Freeze local rotation to kill breathing/idle animations on the shoulders
              rotR[0] = g_VRIKShoulderRotR[0]; rotR[1] = g_VRIKShoulderRotR[1]; rotR[2] = g_VRIKShoulderRotR[2]; rotR[3] = g_VRIKShoulderRotR[3];
              rotL[0] = g_VRIKShoulderRotL[0]; rotL[1] = g_VRIKShoulderRotL[1]; rotL[2] = g_VRIKShoulderRotL[2]; rotL[3] = g_VRIKShoulderRotL[3];

              // 2. Freeze local translation of the clavicles to lock them to the center of the body (Spine)
              float* tClavR = reinterpret_cast<float*>(boneBuf + g_VRRightShoulderIdx * 48 + VRIK_TRANS_OFF);
              float* tClavL = reinterpret_cast<float*>(boneBuf + g_VRLeftShoulderIdx * 48 + VRIK_TRANS_OFF);
              tClavR[0] = g_VRIKShoulderClavOffsetR[0]; tClavR[1] = g_VRIKShoulderClavOffsetR[1]; tClavR[2] = g_VRIKShoulderClavOffsetR[2];
              tClavL[0] = g_VRIKShoulderClavOffsetL[0]; tClavL[1] = g_VRIKShoulderClavOffsetL[1]; tClavL[2] = g_VRIKShoulderClavOffsetL[2];

              // 2. Widen shoulders by scaling the local translation vector of the Upper Arms!
              // This pushes the arm joints further away from the clavicle origin.
              if (g_VRRightUpperArmIdx >= 0 && g_VRLeftUpperArmIdx >= 0) {
                  float* tR = reinterpret_cast<float*>(boneBuf + g_VRRightUpperArmIdx * 48 + VRIK_TRANS_OFF);
                  float* tL = reinterpret_cast<float*>(boneBuf + g_VRLeftUpperArmIdx * 48 + VRIK_TRANS_OFF);
                  
                  tR[0] = g_VRIKShoulderOffsetR[0] * g_VRIKShoulderWidthScale;
                  tR[1] = g_VRIKShoulderOffsetR[1] * g_VRIKShoulderWidthScale;
                  tR[2] = g_VRIKShoulderOffsetR[2] * g_VRIKShoulderWidthScale;

                  tL[0] = g_VRIKShoulderOffsetL[0] * g_VRIKShoulderWidthScale;
                  tL[1] = g_VRIKShoulderOffsetL[1] * g_VRIKShoulderWidthScale;
                  tL[2] = g_VRIKShoulderOffsetL[2] * g_VRIKShoulderWidthScale;
              }
          }

          VRIK_ComputeFK(boneBuf, g_VRBoneCount);
          int hIdx = g_VRHeadBoneIdx;
          const float *headModelPos =
              (hIdx >= 0 && hIdx < VRIK_MAX_BONES) ? g_fkPos[hIdx] : nullptr;

          // HMD orientation relative to recenter base (producer slots 16..19).
          // Used to undo the HMD-local frame of the controller poses.
          float baseHmdRel[4] = { g_pBodyWalkTracking->head.rot[0], g_pBodyWalkTracking->head.rot[1], g_pBodyWalkTracking->head.rot[2], g_pBodyWalkTracking->head.rot[3] };
          
          // Extract yaw angle (-relYaw) from baseHmdRel (which is a Y-axis quaternion)
          // baseHmdRel[1] = sin(angle/2), baseHmdRel[3] = cos(angle/2)
          float relYaw = -2.0f * std::atan2(baseHmdRel[1], baseHmdRel[3]);
          
          // Apply compensation coefficient
          float compYaw = -relYaw * g_VRYawCompensation;
          
          // Re-build quaternion
          float compHmdRel[4] = {
              0.0f,
              std::sin(compYaw * 0.5f),
              0.0f,
              std::cos(compYaw * 0.5f)
          };
          const float *hmdRel = compHmdRel;
          if (headModelPos) {
            // Body frame from FK bone positions -- convention-independent
            // (works regardless of the model's axis layout). bodyUp =
            // root->head, bodyRight = leftShoulder->rightShoulder. These are
            // the stable elbow bend references (see VRIK_SolveArm).
            float bodyUp[3] = {0.0f, 0.0f, 1.0f};
            float bodyRight[3] = {1.0f, 0.0f, 0.0f};
            {
              const float *rootP = g_fkPos[0];
              bodyUp[0] = headModelPos[0] - rootP[0];
              bodyUp[1] = headModelPos[1] - rootP[1];
              bodyUp[2] = headModelPos[2] - rootP[2];
              if (VRIK_Norm3(bodyUp) < 1e-4f) {
                bodyUp[0] = 0.0f;
                bodyUp[1] = 0.0f;
                bodyUp[2] = 1.0f;
              }
              if (g_VRRightUpperArmIdx >= 0 && g_VRLeftUpperArmIdx >= 0) {
                const float *rs = g_fkPos[g_VRRightUpperArmIdx];
                const float *ls = g_fkPos[g_VRLeftUpperArmIdx];
                bodyRight[0] = rs[0] - ls[0];
                bodyRight[1] = rs[1] - ls[1];
                bodyRight[2] = rs[2] - ls[2];
                if (VRIK_Norm3(bodyRight) < 1e-4f) {
                  bodyRight[0] = 1.0f;
                  bodyRight[1] = 0.0f;
                  bodyRight[2] = 0.0f;
                }
              }
            }
            // bodyFwd = up x right (the body's forward/back axis). Sign is
            // oriented by the head bone's forward so the VRArmIK z-term
            // (forward reach) reads positive when the hand is in front of the
            // chest.
            float bodyFwd[3];
            VRIK_Cross3(bodyUp, bodyRight, bodyFwd);
            if (VRIK_Norm3(bodyFwd) < 1e-4f) {
              bodyFwd[0] = 0.0f;
              bodyFwd[1] = 1.0f;
              bodyFwd[2] = 0.0f;
            }
            {
              // Pick the head local axis most aligned with bodyFwd and adopt
              // its sign -> forward points the way the character faces,
              // convention-free.
              const float *hr = g_fkRot[hIdx];
              float best = 0.0f;
              float bestSign = 1.0f;
              const float ax[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
              for (int a = 0; a < 3; ++a) {
                float v[3];
                VRIK_QuatRotateVec(hr, ax[a], v);
                float d =
                    v[0] * bodyFwd[0] + v[1] * bodyFwd[1] + v[2] * bodyFwd[2];
                if (std::fabs(d) > std::fabs(best)) {
                  best = d;
                  bestSign = (d < 0.0f) ? -1.0f : 1.0f;
                }
              }
              bodyFwd[0] *= bestSign;
              bodyFwd[1] *= bestSign;
              bodyFwd[2] *= bestSign;
            }
            // Right arm.
            if (g_pBodyWalkTracking->rightHand.valid > 0) {
              const float vrPos[3] = {g_pBodyWalkTracking->rightHand.pos[0],
                                      g_pBodyWalkTracking->rightHand.pos[1],
                                      g_pBodyWalkTracking->rightHand.pos[2]};
              const float vrQuat[4] = {g_pBodyWalkTracking->rightHand.rot[0],
                                       g_pBodyWalkTracking->rightHand.rot[1],
                                       g_pBodyWalkTracking->rightHand.rot[2],
                                       g_pBodyWalkTracking->rightHand.rot[3]};
              float target[3], handRot[4];
              const float wristR[4] = {g_VRWristR_I, g_VRWristR_J, g_VRWristR_K,
                                       g_VRWristR_R};
              const float offR[3] = {g_VROffRX, g_VROffRY, g_VROffRZ};
              VRIK_BuildHandTarget(headModelPos, hmdRel, vrPos, vrQuat, wristR,
                                   g_VRScaleR, offR, target, handRot);
              VRIK_SolveArm(boneBuf, g_VRRightUpperArmIdx, g_VRRightForeArmIdx,
                            g_VRRightBoneIdx, target, handRot, bodyRight,
                            bodyUp, bodyFwd, g_VRElbowPoleR * 0.01745329252f,
                            g_VRElbowSwingR,
                            /*isLeft*/ false, /*storeDbg*/ true);
            }
            // Left arm (mirror; same shared-memory left slots).
            if (g_pBodyWalkTracking->leftHand.valid > 0 &&
                g_VRLeftUpperArmIdx >= 0) {
              const float vrPos[3] = {g_pBodyWalkTracking->leftHand.pos[0],
                                      g_pBodyWalkTracking->leftHand.pos[1],
                                      g_pBodyWalkTracking->leftHand.pos[2]};
              const float vrQuat[4] = {g_pBodyWalkTracking->leftHand.rot[0],
                                       g_pBodyWalkTracking->leftHand.rot[1],
                                       g_pBodyWalkTracking->leftHand.rot[2],
                                       g_pBodyWalkTracking->leftHand.rot[3]};
              float target[3], handRot[4];
              const float wristL[4] = {g_VRWristL_I, g_VRWristL_J, g_VRWristL_K,
                                       g_VRWristL_R};
              const float offL[3] = {g_VROffLX, g_VROffLY, g_VROffLZ};
              VRIK_BuildHandTarget(headModelPos, hmdRel, vrPos, vrQuat, wristL,
                                   g_VRScaleL, offL, target, handRot);
              VRIK_SolveArm(boneBuf, g_VRLeftUpperArmIdx, g_VRLeftForeArmIdx,
                            g_VRLeftBoneIdx, target, handRot, bodyRight, bodyUp,
                            bodyFwd, g_VRElbowPoleL * 0.01745329252f,
                            g_VRElbowSwingL,
                            /*isLeft*/ true, /*storeDbg*/ true);
            }
          }
        }
        // Legacy direct-write binding (modes 1..3): single-bone hand write.
        else if (g_VRBind > 0 && g_pBodyWalkTracking) {
          // Resolve the head bone pose once (shared by both hands).
          bool headOk = false;
          float headPos[3] = {0.0f, 0.0f, 0.0f};
          float headQuat[4] = {0.0f, 0.0f, 0.0f, 1.0f};
          int hIdx = g_VRHeadBoneIdx;
          if (g_VRUseHeadRelative && hIdx >= 0) {
            const float *hp =
                reinterpret_cast<float *>(boneBuf + hIdx * 48 + VRIK_TRANS_OFF);
            const float *hq =
                reinterpret_cast<float *>(boneBuf + hIdx * 48 + VRIK_ROT_OFF);
            headQuat[0] = hq[0];
            headQuat[1] = hq[1];
            headQuat[2] = hq[2];
            headQuat[3] = hq[3];
            headPos[0] = hp[0];
            headPos[1] = hp[1];
            headPos[2] = hp[2];
            headOk = true;
          }

          // Right Hand (VR slot 8 = valid, 9..11 = pos, 12..15 = quat).
          if (g_pBodyWalkTracking->rightHand.valid > 0) {
            const float vrPos[3] = {g_pBodyWalkTracking->rightHand.pos[0],
                                    g_pBodyWalkTracking->rightHand.pos[1],
                                    g_pBodyWalkTracking->rightHand.pos[2]};
            const float vrQuat[4] = {g_pBodyWalkTracking->rightHand.rot[0],
                                     g_pBodyWalkTracking->rightHand.rot[1],
                                     g_pBodyWalkTracking->rightHand.rot[2],
                                     g_pBodyWalkTracking->rightHand.rot[3]};
            VRIK_WriteHand(boneBuf, g_VRRightBoneIdx, headPos, headQuat, headOk,
                           vrPos, vrQuat, (g_VRBind == 2 || g_VRBind == 3));
          }

          // Left Hand (VR slot 0 = valid, 1..3 = pos, 4..7 = quat).
          if ((g_VRBind == 3) && g_pBodyWalkTracking->leftHand.valid > 0) {
            const float vrPos[3] = {g_pBodyWalkTracking->leftHand.pos[0],
                                    g_pBodyWalkTracking->leftHand.pos[1],
                                    g_pBodyWalkTracking->leftHand.pos[2]};
            const float vrQuat[4] = {g_pBodyWalkTracking->leftHand.rot[0],
                                     g_pBodyWalkTracking->leftHand.rot[1],
                                     g_pBodyWalkTracking->leftHand.rot[2],
                                     g_pBodyWalkTracking->leftHand.rot[3]};
            VRIK_WriteHand(boneBuf, g_VRLeftBoneIdx, headPos, headQuat, headOk,
                           vrPos, vrQuat, true);
          }
        }
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  return result;
}

inline bool InstallVRIKMinHook() {
  HMODULE hMod = GetModuleHandleA("Cyberpunk2077.exe");
  if (!hMod)
    return false;

  // Original Hook
  void *target =
      reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(hMod) + 0x15357A0);
  MH_Initialize();
  if (MH_CreateHook(target, &Hooked_ComponentFunc21,
                    reinterpret_cast<void **>(&OriginalFunc21)) != MH_OK)
    return false;
  if (MH_EnableHook(target) != MH_OK)
    return false;
  return true;
}

inline bool InstallAnimPoseHook() {
  HMODULE hMod = GetModuleHandleA("Cyberpunk2077.exe");
  if (!hMod)
    return false;
  void *target =
      reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(hMod) + 0x17DDB4);
  MH_Initialize(); // no-op if already initialized by InstallVRIKMinHook
  if (MH_CreateHook(target, &Hooked_AnimPoseApply,
                    reinterpret_cast<void **>(&OriginalAnimPose)) != MH_OK)
    return false;
  if (MH_EnableHook(target) != MH_OK)
    return false;
  return true;
}

// ============================================================================
// RAYCAST HOOK: PhysX raycast (RVA 0x1A459C)
// ============================================================================
//
// This is the BOTTOM of all raycast paths. 58 callers converge here.
// 0x1118008 (SyncRaycastByCollisionGroup native) was NOT called during
// hitscan because Shoot() uses a different call chain to reach PhysX.
// Hooking 0x1A459C catches ALL raycasts including hitscan.
//
// Calling convention (from disasm):
//   ecx = scene handle (32-bit, moved from original rcx param)
//   rdx = rayData* (struct with start/end floats)
//   r8  = TraceResult*
//   return: depends on caller's interpretation
//
// Inside the function:
//   0x1A459C: sub rsp, 0x28
//   0x1A45A0: mov r11, rdx     ; save rayData
//   0x1A45A3: mov r10, r8      ; save TraceResult
//   0x1A45A6: mov edx, ecx     ; scene handle -> edx
//   0x1A45A8: mov rcx, [rip+X] ; get physics world
//   0x1A45AF: call 0x23EA70    ; resolve scene
//   0x1A45B7: mov rdx, r11     ; restore rayData
//   0x1A45BA: mov rcx, rax     ; resolved scene
//   0x1A45C1: jmp 0x2389CC     ; tail-call to actual physx query
// ============================================================================

// Control flags — set via CET bridge or RED4ext scripting.
extern volatile int
    g_RaycastTestArmed; // 0=off, 1=log+shift next shot, 2=always shift
extern volatile int g_RaycastLogCount; // how many raycasts were logged
extern volatile float g_RaycastShiftZ;
extern volatile float g_MuzzleX, g_MuzzleY, g_MuzzleZ; // muzzle pos from CET

// Saved player origin from first raycast near player
static float g_ShootOriginX = 0, g_ShootOriginY = 0, g_ShootOriginZ = 0;
static bool g_ShootOriginSet = false;

// ---- Shoot() hook (RVA 0x659C5C) ----
extern volatile int g_InsideShoot;
typedef void (*Shoot_t)(void *rcx, void *rdx, void *r8, void *r9);
static Shoot_t OriginalShoot = nullptr;

extern "C" inline void Hooked_Shoot(void *rcx, void *rdx, void *r8, void *r9) {
  if (g_RaycastTestArmed > 0) {
    __try {
      float* startPoint = (float*)((uintptr_t)rdx + 0xF0);
      float* startVelocity = (float*)((uintptr_t)rdx + 0x100);
      float* targetPosition = (float*)((uintptr_t)rdx + 0x120);

      // Just logging VR hand position to see if it's valid
      // DO NOT OVERRIDE PARAMS YET TO PREVENT CRASH
      float vrX = g_VRIKDbgTarget[0];
      float vrY = g_VRIKDbgTarget[1];
      float vrZ = g_VRIKDbgTarget[2];
      
      // Let's print this to log later!

      FILE *f = fopen("raycast_hook_log.txt", "a");
      if (f) {
        fprintf(f, "\n=== SHOOT EVENT ===\n");
        fprintf(f, "Original startPoint: (%.4f, %.4f, %.4f)\n", startPoint[0], startPoint[1], startPoint[2]);
        fprintf(f, "Original startVelocity: (%.4f, %.4f, %.4f)\n", startVelocity[0], startVelocity[1], startVelocity[2]);
        fprintf(f, "VR Hand: (%.4f, %.4f, %.4f)\n", g_VRIKDbgTarget[0], g_VRIKDbgTarget[1], g_VRIKDbgTarget[2]);
          fprintf(f, "Original targetPosition: (%.4f, %.4f, %.4f)\n", targetPosition[0], targetPosition[1], targetPosition[2]);
        fclose(f);
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    Beep(1000, 50);
  }
  g_InsideShoot = 1;
  OriginalShoot(rcx, rdx, r8, r9);
  g_InsideShoot = 0;
}

typedef void (*Raycast_t)(void* rcx, void* rdx);
Raycast_t OriginalRaycast = nullptr;

extern "C" inline void Hooked_Raycast(void* rcx, void* rdx) {
  if (g_InsideShoot && g_RaycastTestArmed > 0 && rdx) {
    // OVERRIDE THE TRAJECTORY!
    float* startPoint = (float*)((uintptr_t)rdx + 0x40);
    startPoint[2] += g_RaycastShiftZ; // Shift bullet up/down!
    
    float* startVelocity = (float*)((uintptr_t)rdx + 0x60);
    startVelocity[0] = 0.0f;
    startVelocity[1] = 0.0f;
    startVelocity[2] = 1000.0f;
    startVelocity[3] = 0.0f;
  }
  OriginalRaycast(rcx, rdx);
}

typedef bool (*DispatchEvent_t)(void* rcx, void* rdx);
DispatchEvent_t OriginalDispatchEvent = nullptr;

extern "C" inline bool Hooked_DispatchEvent(void* rcx, void* rdx) {
  if (g_InsideShoot && g_RaycastTestArmed > 0 && rdx) {
    FILE *f = fopen("raycast_hook_log.txt", "a");
    if (f) {
      fprintf(f, "\n=== DISPATCH EVENT 0x9D69DC ===\n");
      fprintf(f, "System rcx: %p, Event rdx: %p\n", rcx, rdx);
      __try {
        uint8_t* ptr = (uint8_t*)rdx;
        for(int i=0; i<32; i++) { // dump 512 bytes of event
           fprintf(f, "0x%02X:  %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X\n",
             i*16,
             ptr[i*16+0], ptr[i*16+1], ptr[i*16+2], ptr[i*16+3], ptr[i*16+4], ptr[i*16+5], ptr[i*16+6], ptr[i*16+7],
             ptr[i*16+8], ptr[i*16+9], ptr[i*16+10], ptr[i*16+11], ptr[i*16+12], ptr[i*16+13], ptr[i*16+14], ptr[i*16+15]);
        }
        
        // Let's also print it as floats so it's easier to read!
        float* fptr = (float*)rdx;
        fprintf(f, "As Floats:\n");
        for(int i=0; i<128; i++) {
           if (fptr[i] != 0.0f && !std::isnan(fptr[i]) && !std::isinf(fptr[i])) {
              fprintf(f, "  [%02d]+0x%03X = %.4f\n", i, i * 4, fptr[i]);
           }
        }
      } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "Error reading eventObj memory\n");
      }
      fclose(f);
    }


  }
  return OriginalDispatchEvent(rcx, rdx);
}

inline bool InstallShootHook() {
  HMODULE hMod = GetModuleHandleA("Cyberpunk2077.exe");
  if (!hMod) return false;
  uintptr_t exe_base = reinterpret_cast<uintptr_t>(hMod);

  MH_Initialize();
  
  void *targetShoot = (void *)(exe_base + 0x659C5C);
  if (MH_CreateHook(targetShoot, (void *)&Hooked_Shoot, reinterpret_cast<void **>(&OriginalShoot)) != MH_OK) return false;
  if (MH_EnableHook(targetShoot) != MH_OK) return false;

  void *targetDispatch = (void *)(exe_base + 0x9D69DC);
  if (MH_CreateHook(targetDispatch, (void *)&Hooked_DispatchEvent, reinterpret_cast<void **>(&OriginalDispatchEvent)) == MH_OK) MH_EnableHook(targetDispatch);

  void *targetRaycast = (void *)(exe_base + 0x130989C);
  if (MH_CreateHook(targetRaycast, (void *)&Hooked_Raycast, reinterpret_cast<void **>(&OriginalRaycast)) == MH_OK) MH_EnableHook(targetRaycast);

  return true;
}
