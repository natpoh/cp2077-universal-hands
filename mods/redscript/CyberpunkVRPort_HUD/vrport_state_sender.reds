public native func SendCP2077StateToBodyWalk(stateName: String)

public class BodyWalkPingEvent extends Event {}

@addField(PlayerPuppet)
public let bodyWalkPingDelayId: DelayID;

@addField(PlayerPuppet)
public let bodyWalkIsInMenu: Bool;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
    let result = wrappedMethod();
    this.StartBodyWalkPingLoop();
    return result;
}

@wrapMethod(PlayerPuppet)
protected cb func OnDetach() -> Bool {
    let result = wrappedMethod();
    this.StopBodyWalkPingLoop();
    return result;
}

@addMethod(PlayerPuppet)
public func StartBodyWalkPingLoop() {
    let evt = new BodyWalkPingEvent();
    this.bodyWalkPingDelayId = GameInstance.GetDelaySystem(this.GetGame()).DelayEvent(this, evt, 0.5, false);
}

@addMethod(PlayerPuppet)
public func StopBodyWalkPingLoop() {
    GameInstance.GetDelaySystem(this.GetGame()).CancelDelay(this.bodyWalkPingDelayId);
}

@addMethod(PlayerPuppet)
protected cb func OnBodyWalkPingEvent(evt: ref<BodyWalkPingEvent>) -> Bool {
    this.EvaluateAndPingBodyWalkState();
    
    let nextEvt = new BodyWalkPingEvent();
    this.bodyWalkPingDelayId = GameInstance.GetDelaySystem(this.GetGame()).DelayEvent(this, nextEvt, 0.5, false);
    return true;
}

@addMethod(PlayerPuppet)
public func EvaluateAndPingBodyWalkState() {
    if this.bodyWalkIsInMenu {
        SendCP2077StateToBodyWalk("menu");
        return;
    }

    let psmBlackboard = this.GetPlayerStateMachineBlackboard();
    let stateName = "main";

    if IsDefined(psmBlackboard) {
        let combatState = psmBlackboard.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Combat);
        let vehicleState = psmBlackboard.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vehicle);

        if vehicleState > 0 {
            stateName = "vehicle";
        } 
        else if combatState == 1 {
            stateName = "combat";
        }
    }

    let fullState = stateName + "," + this.GetBodyWalkWeaponCategory();
    SendCP2077StateToBodyWalk(fullState);
}

@wrapMethod(PopupsManager)
protected cb func OnMenuUpdate(isInMenu: Bool) -> Bool {
    let result = wrappedMethod(isInMenu);
    let playerObj = this.GetPlayerControlledObject();
    if IsDefined(playerObj) {
        let player = GameInstance.GetPlayerSystem(playerObj.GetGame()).GetLocalPlayerMainGameObject() as PlayerPuppet;
        if IsDefined(player) {
            player.bodyWalkIsInMenu = isInMenu;
            if isInMenu {
                SendCP2077StateToBodyWalk("menu");
            } else {
                player.EvaluateAndPingBodyWalkState();
            }
        }
    }
    return result;
}

@addMethod(PlayerPuppet)
public final func GetBodyWalkWeaponCategory() -> String {
    let weapon = ScriptedPuppet.GetWeaponRight(this);
    if !IsDefined(weapon) { return "unarmed"; }
    let weaponRecord = weapon.GetWeaponRecord();
    if !IsDefined(weaponRecord) { return "unarmed"; }
    let itemTypeRecord = weaponRecord.ItemType();
    if !IsDefined(itemTypeRecord) { return "unarmed"; }
    let type = itemTypeRecord.Type();
    
    if Equals(type, gamedataItemType.Wea_Fists) || Equals(type, gamedataItemType.Cyb_StrongArms) { return "unarmed"; }
    if Equals(type, gamedataItemType.Wea_Handgun) || Equals(type, gamedataItemType.Wea_Revolver) { return "pistols"; }
    if Equals(type, gamedataItemType.Wea_Shotgun) || Equals(type, gamedataItemType.Wea_ShotgunDual) { return "shotguns"; }
    if Equals(type, gamedataItemType.Wea_AssaultRifle) || Equals(type, gamedataItemType.Wea_Rifle) || Equals(type, gamedataItemType.Wea_SubmachineGun) || Equals(type, gamedataItemType.Wea_SniperRifle) || Equals(type, gamedataItemType.Wea_PrecisionRifle) { return "rifles"; }
    if Equals(type, gamedataItemType.Wea_HeavyMachineGun) || Equals(type, gamedataItemType.Wea_LightMachineGun) || Equals(type, gamedataItemType.Wea_GrenadeLauncher) || Equals(type, gamedataItemType.Cyb_Launcher) { return "heavy"; }
    return "melee";
}

@wrapMethod(PlayerPuppet)
public final func OnItemEquipped(slot: TweakDBID, item: ItemID) -> Void {
    wrappedMethod(slot, item);
    if slot == t"AttachmentSlots.WeaponRight" {
        this.EvaluateAndPingBodyWalkState();
    }
}

@wrapMethod(PlayerPuppet)
public final func OnItemUnequipped(slot: TweakDBID, item: ItemID) -> Void {
    wrappedMethod(slot, item);
    if slot == t"AttachmentSlots.WeaponRight" {
        this.EvaluateAndPingBodyWalkState();
    }
}
