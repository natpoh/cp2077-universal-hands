
public native func SetVRBindParams(scale: Float, x: Float, y: Float, z: Float, axis: Int32, hand: Int32) -> Int32;
public native func SetVRElbowSwing(angle: Float, hand: Int32) -> Int32;
public native func SetVRElbowPole(angle: Float, hand: Int32) -> Int32;
public native func SetVRHandOffset(pitch: Float, yaw: Float, roll: Float, hand: Int32) -> Int32;
public native func InstallVRAnimPoseHook() -> Int32;
public native func ArmVRAnimPosePlayer() -> Int32;
public native func SetVRBindMode(mode: Int32) -> Int32;
public native func SetVRShoulderWidth(scale: Float) -> Int32;

public class CyberpunkUniversalHandsVRSettings extends IScriptable {

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "General")
  @runtimeProperty("ModSettings.category.order", "1")
  @runtimeProperty("ModSettings.displayName", "Enable Universal Hands")
  let enabled: Bool = false;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Body Tuning")
  @runtimeProperty("ModSettings.category.order", "50")
  @runtimeProperty("ModSettings.displayName", "Shoulder Width Scale")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "1.0")
  @runtimeProperty("ModSettings.max", "3.0")
  let shoulderWidth: Float = 2.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Reach Scale")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.5")
  @runtimeProperty("ModSettings.max", "2.0")
  let scaleR: Float = 1.05;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Left/Right Offset")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "-1.0")
  @runtimeProperty("ModSettings.max", "1.0")
  let xOffR: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Forward Offset")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "-1.0")
  @runtimeProperty("ModSettings.max", "1.0")
  let yOffR: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Height Offset")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "-1.0")
  @runtimeProperty("ModSettings.max", "1.0")
  let zOffR: Float = 0.23;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Elbow Swing Gain")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.min", "-2.0")
  @runtimeProperty("ModSettings.max", "2.0")
  let swingR: Float = 1.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Elbow Pole Spin")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "-180.0")
  @runtimeProperty("ModSettings.max", "180.0")
  let poleR: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Wrist Pitch")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "360.0")
  let pitchR: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Wrist Yaw")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "360.0")
  let yawR: Float = 270.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Right Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Wrist Roll")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "360.0")
  let rollR: Float = 0.0;

  // ==================== LEFT HAND ==================== //

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Reach Scale")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "0.5")
  @runtimeProperty("ModSettings.max", "2.0")
  let scaleL: Float = 1.06;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Left/Right Offset")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "-1.0")
  @runtimeProperty("ModSettings.max", "1.0")
  let xOffL: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Forward Offset")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "-1.0")
  @runtimeProperty("ModSettings.max", "1.0")
  let yOffL: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Height Offset")
  @runtimeProperty("ModSettings.step", "0.01")
  @runtimeProperty("ModSettings.min", "-1.0")
  @runtimeProperty("ModSettings.max", "1.0")
  let zOffL: Float = 0.23;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Elbow Swing Gain")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.min", "-2.0")
  @runtimeProperty("ModSettings.max", "2.0")
  let swingL: Float = 1.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Elbow Pole Spin")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "-180.0")
  @runtimeProperty("ModSettings.max", "180.0")
  let poleL: Float = 0.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Wrist Pitch")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "360.0")
  let pitchL: Float = 180.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Wrist Yaw")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "360.0")
  let yawL: Float = 270.0;

  @runtimeProperty("ModSettings.mod", "Cyberpunk Universal Hands VR")
  @runtimeProperty("ModSettings.category", "Left Hand Calibration")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Wrist Roll")
  @runtimeProperty("ModSettings.step", "1.0")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "360.0")
  let rollL: Float = 0.0;

  public func OnModSettingsChange() -> Void {
    this.Push();
  }

  public func Push() -> Void {
    SetVRBindParams(this.scaleL, this.xOffL, this.yOffL, this.zOffL, 0, 1);
    SetVRBindParams(this.scaleR, this.xOffR, this.yOffR, this.zOffR, 0, 0);

    SetVRElbowSwing(this.swingL, 1);
    SetVRElbowSwing(this.swingR, 0);

    SetVRElbowPole(this.poleL, 1);
    SetVRElbowPole(this.poleR, 0);

    SetVRHandOffset(this.pitchL, this.yawL, this.rollL, 1);
    SetVRHandOffset(this.pitchR, this.yawR, this.rollR, 0);

    SetVRShoulderWidth(this.shoulderWidth);

    if this.enabled {
      InstallVRAnimPoseHook();
      ArmVRAnimPosePlayer();
      SetVRBindMode(4);
    } else {
      SetVRBindMode(0);
    }
  }
}

@addField(PlayerPuppet)
public let m_universalHandsSettings: ref<CyberpunkUniversalHandsVRSettings>;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  if !IsDefined(this.m_universalHandsSettings) {
    this.m_universalHandsSettings = new CyberpunkUniversalHandsVRSettings();
    ModSettings.RegisterListenerToClass(this.m_universalHandsSettings);
    ModSettings.RegisterListenerToModifications(this.m_universalHandsSettings);
    this.m_universalHandsSettings.Push();
  }
  return result;
}
