public native func SendCP2077StateToBodyWalk(stateName: String)

@addField(PlayerPuppet)
public let bodyWalkLastNonMenuState: String;

@addField(PlayerPuppet)
public let bodyWalkIsInMenu: Bool;

@addMethod(PlayerPuppet)
public func SetBodyWalkState(stateName: String) {
    this.bodyWalkLastNonMenuState = stateName;
    if !this.bodyWalkIsInMenu {
        let fullState = stateName + "," + this.GetBodyWalkWeaponCategory();
        SendCP2077StateToBodyWalk(fullState);
    }
}

@wrapMethod(PlayerPuppet)
protected cb func OnCombatStateChanged(state: Int32) -> Bool {
    let result = wrappedMethod(state);
    // 0 = Out of combat, 1 = In combat, 2 = Stealth
    if state == 1 {
        this.SetBodyWalkState("combat");
    } else {
        this.SetBodyWalkState("main");
    }
    return result;
}

@wrapMethod(VehicleComponent)
protected cb func OnVehicleFinishedMountingEvent(evt: ref<VehicleFinishedMountingEvent>) -> Bool {
    let result = wrappedMethod(evt);
    let character: ref<GameObject> = evt.character as GameObject;
    let player = character as PlayerPuppet;
    if IsDefined(player) && evt.isMounting {
        player.SetBodyWalkState("vehicle");
    }
    return result;
}

@wrapMethod(VehicleComponent)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
    let result = wrappedMethod(evt);
    let game: GameInstance = this.GetVehicle().GetGame();
    let childId: EntityID = evt.request.lowLevelMountingInfo.childId;
    let mountChild: ref<GameObject> = GameInstance.FindEntityByID(game, childId) as GameObject;
    let player = mountChild as PlayerPuppet;
    if IsDefined(player) {
        player.SetBodyWalkState("main");
    }
    return result;
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
                let lastState = player.bodyWalkLastNonMenuState;
                if Equals(lastState, "") {
                    lastState = "main";
                }
                let fullState = lastState + "," + player.GetBodyWalkWeaponCategory();
                SendCP2077StateToBodyWalk(fullState);
            }
        }
    }
    return result;
}

@addMethod(PlayerPuppet)
public final func GetBodyWalkWeaponCategory() -> String {
    let weapon = ScriptedPuppet.GetWeaponRight(this);
    if !IsDefined(weapon) {
        return "unarmed";
    }
    let weaponRecord = weapon.GetWeaponRecord();
    if !IsDefined(weaponRecord) {
        return "unarmed";
    }
    let itemTypeRecord = weaponRecord.ItemType();
    if !IsDefined(itemTypeRecord) {
        return "unarmed";
    }
    let type = itemTypeRecord.Type();
    
    if Equals(type, gamedataItemType.Wea_Fists) || Equals(type, gamedataItemType.Cyb_StrongArms) {
        return "unarmed";
    }
    if Equals(type, gamedataItemType.Wea_Handgun) || Equals(type, gamedataItemType.Wea_Revolver) {
        return "pistols";
    }
    if Equals(type, gamedataItemType.Wea_Shotgun) || Equals(type, gamedataItemType.Wea_ShotgunDual) {
        return "shotguns";
    }
    if Equals(type, gamedataItemType.Wea_AssaultRifle) || Equals(type, gamedataItemType.Wea_Rifle) || Equals(type, gamedataItemType.Wea_SubmachineGun) || Equals(type, gamedataItemType.Wea_SniperRifle) || Equals(type, gamedataItemType.Wea_PrecisionRifle) {
        return "rifles";
    }
    if Equals(type, gamedataItemType.Wea_HeavyMachineGun) || Equals(type, gamedataItemType.Wea_LightMachineGun) || Equals(type, gamedataItemType.Wea_GrenadeLauncher) || Equals(type, gamedataItemType.Cyb_Launcher) {
        return "heavy";
    }
    return "melee";
}

@wrapMethod(PlayerPuppet)
public final func OnItemEquipped(slot: TweakDBID, item: ItemID) -> Void {
    wrappedMethod(slot, item);
    if slot == t"AttachmentSlots.WeaponRight" {
        let lastState = this.bodyWalkLastNonMenuState;
        if Equals(lastState, "") {
            lastState = "main";
        }
        this.SetBodyWalkState(lastState);
    }
}

@wrapMethod(PlayerPuppet)
public final func OnItemUnequipped(slot: TweakDBID, item: ItemID) -> Void {
    wrappedMethod(slot, item);
    if slot == t"AttachmentSlots.WeaponRight" {
        let lastState = this.bodyWalkLastNonMenuState;
        if Equals(lastState, "") {
            lastState = "main";
        }
        this.SetBodyWalkState(lastState);
    }
}
