// bodywalk_actions.reds
// Directly executes game logic functions based on VR Actions

public native func GetVRActionJustPressed(actionId: Int32) -> Bool
public native func GetVRActionState(actionId: Int32) -> Bool

@addMethod(PlayerPuppet)
public func ProcessBodyWalkVRActions() {
    let eqSystem = EquipmentSystem.GetInstance(this);
    let eqData = EquipmentSystem.GetData(this);

    // Action 3: Next Weapon
    if GetVRActionJustPressed(3) {
        let request = new EquipmentSystemWeaponManipulationRequest();
        request.requestType = EquipmentManipulationAction.CycleNextWeaponWheelItem;
        request.owner = this;
        eqSystem.QueueRequest(request);
    }
    
    // Action 4: Holster Weapon
    if GetVRActionJustPressed(4) {
        let request = new EquipmentSystemWeaponManipulationRequest();
        request.requestType = EquipmentManipulationAction.UnequipWeapon;
        request.owner = this;
        eqSystem.QueueRequest(request);
    }
    
    // Action 5: Use Consumable (Healing)
    if GetVRActionJustPressed(5) {
        if IsDefined(eqData) {
            let itemID = eqData.GetActiveConsumable();
            if ItemID.IsValid(itemID) {
                ItemActionsHelper.ConsumeItem(this, itemID, true);
            }
        }
    }
    // Action 0: Draw Weapon 1
    if GetVRActionJustPressed(0) {
        let request = new EquipmentSystemWeaponManipulationRequest();
        request.requestType = EquipmentManipulationAction.RequestWeaponSlot1;
        request.owner = this;
        eqSystem.QueueRequest(request);
    }

    // Action 1: Draw Weapon 2
    if GetVRActionJustPressed(1) {
        let request = new EquipmentSystemWeaponManipulationRequest();
        request.requestType = EquipmentManipulationAction.RequestWeaponSlot2;
        request.owner = this;
        eqSystem.QueueRequest(request);
    }

    // Action 2: Draw Weapon 3
    if GetVRActionJustPressed(2) {
        let request = new EquipmentSystemWeaponManipulationRequest();
        request.requestType = EquipmentManipulationAction.RequestWeaponSlot3;
        request.owner = this;
        eqSystem.QueueRequest(request);
    }
}
