// ------------------------------------------------------------------------------------------------
// SCR_ClutchControllerComponent.c
// ------------------------------------------------------------------------------------------------

// 1. DATA CLASSES
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_sName")]
class SCR_GearboxPattern
{
	[Attribute("New Pattern", UIWidgets.EditBox)]
	string m_sName;
	[Attribute("", UIWidgets.EditBox, "Vehicle Prefab Names")]
	ref array<string> m_aVehicleFilters;
	[Attribute("", UIWidgets.Object)]
	ref array<ref SCR_GearSlot> m_aGearSlots;
	[Attribute("", UIWidgets.EditBox, "Image Name (Quad)")]
	string m_sQuadName;
	[Attribute("", UIWidgets.ResourceNamePicker, "Custom Imageset", "imageset")]
	ResourceName m_sCustomImageset;
	[Attribute("1.0", UIWidgets.Slider, "Sensitivity Multiplier", "0.1 5.0 0.1")]
	float m_fSensitivityMultiplier;
	[Attribute("76.0", UIWidgets.Slider, "Throw Radius", "30.0 150.0 1.0")]
	float m_fThrowRadius;
	[Attribute("50.0", UIWidgets.Slider, "Gate Offset", "10.0 200.0 1.0")]
	float m_fGateOffset;
	[Attribute("20.0", UIWidgets.Slider, "Channel Width", "5.0 100.0 1.0")]
	float m_fChannelWidth;
	[Attribute("35.0", UIWidgets.Slider, "Gear Trigger Height", "5.0 100.0 1.0")]
	float m_fGearTriggerHeight;
	[Attribute("20.0", UIWidgets.Slider, "Neutral Height", "5.0 100.0 1.0")]
	float m_fNeutralHeight;
	[Attribute("50.0", UIWidgets.Slider, "Neutral Extension", "0.0 200.0 1.0")]
	float m_fNeutralExtension;
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_iGearIndex")]
class SCR_GearSlot
{
	[Attribute("1", UIWidgets.EditBox, "Gear Index (-1=Rev, 0=Neutral, 1=1st...)")]
	int m_iGearIndex;
	[Attribute("-1.0 -1.0 0.0", UIWidgets.Coords, "GRID POS: X=Lane | Y=Direction")]
	vector m_vPosition;
	[Attribute("-1.0", UIWidgets.Slider, "Override: Sensitivity", "-1.0 5.0 0.1")]
	float m_fSensitivity;
	[Attribute("-1.0", UIWidgets.Slider, "Override: Throw Radius", "-1.0 150.0 1.0")]
	float m_fThrowRadius;
	[Attribute("-1.0", UIWidgets.Slider, "Override: Channel Width", "-1.0 100.0 1.0")]
	float m_fChannelWidth;
	[Attribute("-1.0", UIWidgets.Slider, "Override: Trigger Height", "-1.0 100.0 1.0")]
	float m_fTriggerHeight;
}

[BaseContainerProps(configRoot: true), SCR_BaseContainerCustomTitleUIInfo("m_sConfigName")]
class SCR_ClutchConfig
{
	[Attribute("Global Clutch Config", UIWidgets.EditBox)]
	string m_sConfigName;
	[Attribute("", UIWidgets.Object, "List of all Shift Patterns")]
	ref array<ref SCR_GearboxPattern> m_aShiftPatterns;
}

// 2. COMPONENT CLASS
[ComponentEditorProps(category: "GameScripted/Vehicle", description: "Defines Manual Shifting Pattern.")]
class SCR_ManualTransmissionComponentClass : ScriptComponentClass {}

class SCR_ManualTransmissionComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object, "Vehicle Pattern")]
	ref SCR_GearboxPattern m_VehiclePattern;
}

// 3. CONTROLLER CLASS
modded class SCR_PlayerController : PlayerController
{
	// --- CONFIGURATION ---
	[Attribute("{E5CB8EC5EB63BC54}Configs/UI_ClutchDisplay.layout", UIWidgets.ResourceNamePicker, "UI Layout")]
	protected ResourceName m_sClutchLayout;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "Imageset", "imageset")]
	protected ResourceName m_sClutchImageset;

	[Attribute("", UIWidgets.EditBox, "Default Quad")]
	protected string m_sDefaultQuadName;

	[Attribute("1.0", UIWidgets.Slider, "Global Sensitivity")]
	protected float m_fBaseSensitivity;
	
	[Attribute("0.4", UIWidgets.Slider, "Inactive Opacity")]
	protected float m_fInactiveOpacity;

	[Attribute("", UIWidgets.ResourceNamePicker, "Global Config File", "conf")]
	protected ResourceName m_sClutchConfigFile;

	// --- VARIABLES ---
	protected Widget m_wRoot;
	protected ImageWidget m_wClutchCircle;
	protected ImageWidget m_wPatternBackground;
	
	protected float m_fClutchX = 0.0;
	protected float m_fClutchY = 0.0;
	protected int m_iCurrentGear = 0; 
	protected bool m_bWasPressed = false;
	protected float m_fLastHorizontalPos = 0.0;
	
	protected int m_iLastSyncedGear = -2;
	protected float m_fCachedMinIdx = 0;
	protected float m_fCachedMaxIdx = 0;
	
	protected SCR_CompartmentAccessComponent m_CompartmentAccess;
	protected RplComponent m_RplComponent;
	protected CarControllerComponent m_CarController;
	
	protected ref SCR_ClutchConfig m_ClutchConfigObject;
	protected ref SCR_GearboxPattern m_CurrentPattern;

	// --- ANIMATION VARIABLES ---
	protected int m_iAnimVarClutch = -1;
	protected bool m_bAnimBindingsInit = false;

	// Defaults
	protected float m_fDefOriginX = -14.0;
	protected float m_fDefOriginY = -110.0;
	protected float m_fDefRadius = 76.0;
	protected float m_fDefGateOffset = 50.0;
	protected float m_fDefChanWidth = 20.0;
	protected float m_fDefNeutralHeight = 20.0;

	// --- INITIALIZATION ---
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		m_fClutchX = m_fDefOriginX;
		m_fClutchY = m_fDefOriginY;
		m_fLastHorizontalPos = m_fDefOriginX;
		
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		
		if (m_sClutchConfigFile != "")
		{
			Resource resource = Resource.Load(m_sClutchConfigFile);
			if (resource && resource.IsValid())
			{
				m_ClutchConfigObject = SCR_ClutchConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			}
		}
		
		HookEvents(GetControlledEntity());
	}
	
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		HookEvents(to);
		m_bAnimBindingsInit = false;
	}
	
	protected void HookEvents(IEntity entity)
	{
		if (m_CompartmentAccess)
		{
			m_CompartmentAccess.GetOnCompartmentEntered().Remove(OnSeatChange);
			m_CompartmentAccess.GetOnCompartmentLeft().Remove(OnSeatChange);
			m_CompartmentAccess = null;
		}
		
		m_CarController = null;
		
		if (!entity) return;

		m_CompartmentAccess = SCR_CompartmentAccessComponent.Cast(entity.FindComponent(SCR_CompartmentAccessComponent));
		
		if (m_CompartmentAccess)
		{
			m_CompartmentAccess.GetOnCompartmentEntered().Insert(OnSeatChange);
			m_CompartmentAccess.GetOnCompartmentLeft().Insert(OnSeatChange);
			CheckDriverStatus();
		}
	}

	protected void OnSeatChange(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (targetEntity)
		{
			BaseCompartmentSlot slot = manager.FindCompartment(slotID, mgrID);
			if (slot)
			{
				IEntity vehicle = slot.GetOwner();
				if (vehicle)
				{
					m_CarController = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
					m_bAnimBindingsInit = false;
				}
			}
		}

		CheckDriverStatus();
	}
	
	protected void CheckDriverStatus()
	{
		if (!IsProxy() && IsPlayerDriver())
		{
			ResolveShiftPattern();
			m_iLastSyncedGear = -2; 
			
			if (m_CurrentPattern) 
			{
				if (!m_wRoot) 
				{
					CreateClutchUI();
					SyncUIWithVehicle(); 
					if (m_CarController)
						m_CarController.SetDrivingAssistanceMode(EVehicleDrivingAssistanceMode.NONE);
				}
			}
			else
			{
				if (m_wRoot) { m_wRoot.RemoveFromHierarchy(); m_wRoot = null; }
			}
		}
		else
		{
			if (m_wRoot) { m_wRoot.RemoveFromHierarchy(); m_wRoot = null; }
		}
	}
	
	protected void ResolveShiftPattern()
	{
		m_CurrentPattern = null;
		IEntity vehicle = GetVehicle();
		if (!vehicle) return;

		SCR_ManualTransmissionComponent modderComp = SCR_ManualTransmissionComponent.Cast(vehicle.FindComponent(SCR_ManualTransmissionComponent));
		if (modderComp && modderComp.m_VehiclePattern)
		{
			m_CurrentPattern = modderComp.m_VehiclePattern;
			CachePatternData(); 
			return; 
		}
		
		if (!m_ClutchConfigObject) return;
		
		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (!prefabData) return;
		
		if (m_ClutchConfigObject.m_aShiftPatterns)
		{
			foreach (SCR_GearboxPattern pattern : m_ClutchConfigObject.m_aShiftPatterns)
			{
				foreach (string filter : pattern.m_aVehicleFilters)
				{
					if (prefabData.GetPrefabName().Contains(filter))
					{
						m_CurrentPattern = pattern;
						CachePatternData(); 
						return; 
					}
				}
			}
		}
	}
	
	protected void CachePatternData()
	{
		if (!m_CurrentPattern) return;
		float minI = 999.0;
		float maxI = -999.0;
		foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
		{
			float idx = slot.m_vPosition[0];
			if (idx < minI) minI = idx;
			if (idx > maxI) maxI = idx;
		}
		m_fCachedMinIdx = minI;
		m_fCachedMaxIdx = maxI;
	}
	
	protected IEntity GetVehicle()
	{
		if (!m_CompartmentAccess) return null;
		BaseCompartmentSlot slot = m_CompartmentAccess.GetCompartment();
		if (!slot) return null;
		return slot.GetOwner();
	}

	// ------------------------------------------------------------------------------------------------
	// FRAME LOOP
	// ------------------------------------------------------------------------------------------------

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if (!m_CurrentPattern) return;
		if (!m_CarController) return; 

		if (m_CarController)
		{
			if (m_CarController.GetDrivingAssistanceMode() != EVehicleDrivingAssistanceMode.NONE)
				m_CarController.SetDrivingAssistanceMode(EVehicleDrivingAssistanceMode.NONE);
		}

		if (IsProxy()) return;
		if (!m_wRoot) return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager) return;
		
		float buttonVal = inputManager.GetActionValue("Clutch_Engage");
		bool isPressed = (buttonVal > 0);

		// --- CLUTCH PHYSICS & FOOT ANIMATION ---
		// We use CallLater to fight the native controller reset
		if (isPressed)
		{
			GetGame().GetCallqueue().CallLater(ApplyClutchToVehicle, 0, false, 1.0);
		}
		else if (m_bWasPressed)
		{
			GetGame().GetCallqueue().CallLater(ApplyClutchToVehicle, 0, false, 0.0);
		}

		if (isPressed) 
		{
			// ON PRESS (One time): Reset stick to match current engine gear
			if (!m_bWasPressed) 
			{
				SyncUIWithVehicle();
			}
			
			m_wRoot.SetOpacity(1.0);
			inputManager.ActivateContext("Clutch_Context");
			
			// WHILE HOLDING: Update stick freely (mouse control)
			UpdateClutchCircle(inputManager, timeSlice);
		}
		else
		{
			if (m_bWasPressed) 
			{
				// ON RELEASE (One time): Lock in the selected gear
				m_fLastHorizontalPos = m_fClutchX;
				
				ApplyGearToVehicle(m_iCurrentGear);
				
				if (m_iCurrentGear != m_iLastSyncedGear)
				{
					Rpc(Rpc_SetGear, m_iCurrentGear);
					m_iLastSyncedGear = m_iCurrentGear;
				}
			}
			m_wRoot.SetOpacity(m_fInactiveOpacity);
		}
		
		m_bWasPressed = isPressed;
	}
	
	// --- HELPERS ---
	
	protected void InitAnimBindings(CharacterAnimationComponent animComp)
	{
		if (m_bAnimBindingsInit) return;
		
		// Only binding Clutch Foot variable now
		m_iAnimVarClutch = animComp.BindVariableFloat("VehicleClutch");
		if (m_iAnimVarClutch == -1) m_iAnimVarClutch = animComp.BindVariableFloat("Clutch");
		
		m_bAnimBindingsInit = true;
	}
	
	protected void ApplyClutchToVehicle(float clutchAmount)
	{
		if (!m_CarController) return;
		VehicleWheeledSimulation simulation = m_CarController.GetWheeledSimulation();
		if (!simulation) return;
		
		simulation.SetClutch(clutchAmount);
		
		// Update Foot Visual
		UpdateCharacterClutch(clutchAmount);
	}
	
	protected void UpdateCharacterClutch(float value)
	{
		IEntity driver = GetControlledEntity();
		if (!driver) return;
		CharacterAnimationComponent animComp = CharacterAnimationComponent.Cast(driver.FindComponent(CharacterAnimationComponent));
		if (!animComp) return;
		
		InitAnimBindings(animComp);
		if (m_iAnimVarClutch != -1) animComp.SetVariableFloat(m_iAnimVarClutch, value);
	}

	// --- UI & PHYSICS ---

	protected void CreateClutchUI()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		if (!workspace) return;

		m_wRoot = workspace.CreateWidgets(m_sClutchLayout);
		if (m_wRoot)
		{
			m_wClutchCircle = ImageWidget.Cast(m_wRoot.FindWidget("ClutchCircle"));
			m_wPatternBackground = ImageWidget.Cast(m_wRoot.FindWidget("PatternBackground"));
			
			string quadName = m_sDefaultQuadName;
			ResourceName targetImageset = m_sClutchImageset; 
			
			if (m_CurrentPattern)
			{
				if (m_CurrentPattern.m_sQuadName != "") quadName = m_CurrentPattern.m_sQuadName;
				if (m_CurrentPattern.m_sCustomImageset != "") targetImageset = m_CurrentPattern.m_sCustomImageset;
			}
			
			if (m_wPatternBackground && targetImageset != "" && quadName != "")
			{
				m_wPatternBackground.LoadImageFromSet(0, targetImageset, quadName);
			}
		}
	}

	protected void SyncUIWithVehicle()
	{
		if (!m_CarController) return;

		VehicleWheeledSimulation simulation = m_CarController.GetWheeledSimulation();
		if (!simulation) return;

		int engineGear = simulation.GetGear();
		if (engineGear == 0) m_iCurrentGear = -1;
		else m_iCurrentGear = engineGear - 1; 

		float targetX = m_fDefOriginX;
		float targetY = m_fDefOriginY;
		
		if (m_CurrentPattern)
		{
			float gateOffset = m_CurrentPattern.m_fGateOffset;
			bool found = false;
			foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
			{
				if (slot.m_iGearIndex == m_iCurrentGear)
				{
					float chIndex = slot.m_vPosition[0];
					float dir = slot.m_vPosition[1];
					float rad = m_CurrentPattern.m_fThrowRadius;
					if (slot.m_fThrowRadius > 0) rad = slot.m_fThrowRadius;
					
					targetX = m_fDefOriginX + (chIndex * gateOffset);
					targetY = m_fDefOriginY + (dir * rad);
					found = true;
					break;
				}
			}
			if (!found && m_iCurrentGear == 0)
			{
				targetX = SnapToClosestChannel(m_fLastHorizontalPos, gateOffset);
				targetY = m_fDefOriginY;
			}
		}
		else
		{
			float leftX   = m_fDefOriginX - m_fDefGateOffset;
			float rightX  = m_fDefOriginX + m_fDefGateOffset;
			float topY    = m_fDefOriginY - m_fDefRadius; 
			float botY    = m_fDefOriginY + m_fDefRadius;

			if (m_iCurrentGear == 0)      { targetX = SnapToClosestChannel(m_fLastHorizontalPos, m_fDefGateOffset); targetY = m_fDefOriginY; } 
			else if (m_iCurrentGear == 1) { targetX = leftX; targetY = topY; } 
			else if (m_iCurrentGear == 2) { targetX = leftX; targetY = botY; } 
			else if (m_iCurrentGear == 3) { targetX = m_fDefOriginX; targetY = topY; } 
			else if (m_iCurrentGear == 4) { targetX = m_fDefOriginX; targetY = botY; } 
			else if (m_iCurrentGear == 5) { targetX = rightX; targetY = topY; } 
			else if (m_iCurrentGear == -1){ targetX = rightX; targetY = botY; }
		}
		
		m_fClutchX = targetX;
		m_fClutchY = targetY;
		m_fLastHorizontalPos = m_fClutchX;
		
		if (m_wClutchCircle) FrameSlot.SetPos(m_wClutchCircle, m_fClutchX, m_fClutchY);
	}

	protected void UpdateClutchCircle(InputManager inputManager, float timeSlice)
	{
		if (!m_wClutchCircle) return;

		float baseSens = m_fBaseSensitivity;
		float gateOffset = m_fDefGateOffset;
		float neutralExtension = 50.0; 
		float neutHeight = m_fDefNeutralHeight;

		if (m_CurrentPattern)
		{
			baseSens = m_fBaseSensitivity * m_CurrentPattern.m_fSensitivityMultiplier;
			gateOffset = m_CurrentPattern.m_fGateOffset;
			neutralExtension = m_CurrentPattern.m_fNeutralExtension;
			neutHeight = m_CurrentPattern.m_fNeutralHeight;
		}

		float mouseX = inputManager.GetActionValue("Clutch_X");
		float mouseY = inputManager.GetActionValue("Clutch_Y");
		
		float nearestLaneIdx = Math.Round((m_fClutchX - m_fDefOriginX) / gateOffset);
		
		float lookDir = 0; 
		if (m_fClutchY < -5.0) lookDir = -1; 
		else if (m_fClutchY > 5.0) lookDir = 1; 
		else if (mouseY < -0.1) lookDir = -1; 
		else if (mouseY > 0.1) lookDir = 1; 
		
		float activeSens = baseSens;
		float activeThrowRad = m_fDefRadius;
		float activeChanWidth = m_fDefChanWidth;
		
		if (m_CurrentPattern)
		{
			activeThrowRad = m_CurrentPattern.m_fThrowRadius;
			activeChanWidth = m_CurrentPattern.m_fChannelWidth;
			
			if (lookDir != 0)
			{
				foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
				{
					if (Math.AbsFloat(slot.m_vPosition[0] - nearestLaneIdx) < 0.1 && slot.m_vPosition[1] == lookDir)
					{
						if (slot.m_fSensitivity > 0) activeSens = m_fBaseSensitivity * slot.m_fSensitivity;
						if (slot.m_fThrowRadius > 0) activeThrowRad = slot.m_fThrowRadius;
						if (slot.m_fChannelWidth > 0) activeChanWidth = slot.m_fChannelWidth;
						break;
					}
				}
			}
		}

		float pendingX = m_fClutchX + (mouseX * activeSens);
		float pendingY = m_fClutchY + (mouseY * activeSens);
		
		float chanHalf = activeChanWidth * 0.5;
		float neutHalf = neutHeight * 0.5;
		float neutralTop    = m_fDefOriginY - neutHalf;
		float neutralBottom = m_fDefOriginY + neutHalf;
		
		float minLaneX = m_fDefOriginX;
		float maxLaneX = m_fDefOriginX;
		
		if (m_CurrentPattern)
		{
			minLaneX = m_fDefOriginX + (m_fCachedMinIdx * gateOffset);
			maxLaneX = m_fDefOriginX + (m_fCachedMaxIdx * gateOffset);
		}
		else
		{
			minLaneX = m_fDefOriginX - gateOffset;
			maxLaneX = m_fDefOriginX + gateOffset;
		}

		bool currentlyInNeutral = (m_fClutchY >= neutralTop && m_fClutchY <= neutralBottom);
		
		if (currentlyInNeutral)
		{
			pendingX = Math.Clamp(pendingX, minLaneX - neutralExtension, maxLaneX + neutralExtension);
		}
		else
		{
			float laneCenterX = m_fDefOriginX + (nearestLaneIdx * gateOffset);
			float widthMin = laneCenterX - chanHalf;
			float widthMax = laneCenterX + chanHalf;
			bool hasLeft = (nearestLaneIdx > m_fCachedMinIdx);
			bool hasRight = (nearestLaneIdx < m_fCachedMaxIdx);
			float leftNeighborX = m_fDefOriginX + ((nearestLaneIdx - 1) * gateOffset);
			float midLeft = (laneCenterX + leftNeighborX) * 0.5;
			float rightNeighborX = m_fDefOriginX + ((nearestLaneIdx + 1) * gateOffset);
			float midRight = (laneCenterX + rightNeighborX) * 0.5;
			float clampMin = widthMin;
			float clampMax = widthMax;
			if (hasLeft) clampMin = Math.Max(widthMin, midLeft + 0.1); 
			if (hasRight) clampMax = Math.Min(widthMax, midRight - 0.1);

			pendingX = Math.Clamp(pendingX, clampMin, clampMax);
		}

		m_fClutchX = pendingX;

		float padding = 20.0;
		pendingY = Math.Clamp(pendingY, m_fDefOriginY - (activeThrowRad + padding), m_fDefOriginY + (activeThrowRad + padding));

		float nearestLaneX = m_fDefOriginX + (nearestLaneIdx * gateOffset);
		float distToLane = Math.AbsFloat(m_fClutchX - nearestLaneX);
		bool isAligned = (distToLane <= chanHalf);
		
		if (!isAligned)
		{
			pendingY = Math.Clamp(pendingY, neutralTop, neutralBottom);
		}
		else
		{
			bool blockTop = true;
			bool blockBot = true;
			if (m_CurrentPattern)
			{
				foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
				{
					if (Math.AbsFloat(slot.m_vPosition[0] - nearestLaneIdx) < 0.1)
					{
						if (slot.m_vPosition[1] < 0) blockTop = false; 
						if (slot.m_vPosition[1] > 0) blockBot = false; 
					}
				}
			}
			else
			{
				blockTop = false; blockBot = false;
			}
			
			if (pendingY < neutralTop && blockTop) pendingY = neutralTop;
			if (pendingY > neutralBottom && blockBot) pendingY = neutralBottom;
		}

		m_fClutchY = pendingY;
		
		FrameSlot.SetPos(m_wClutchCircle, m_fClutchX, m_fClutchY);
		CheckGearShift();
	}

	protected void CheckGearShift()
	{
		int selectedGear = 0; 
		if (m_CurrentPattern)
		{
			float gateOffset = m_CurrentPattern.m_fGateOffset;
			foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
			{
				float idx = slot.m_vPosition[0];
				float dir = slot.m_vPosition[1];
				float rad = m_CurrentPattern.m_fThrowRadius;
				float trigH = m_CurrentPattern.m_fGearTriggerHeight;
				float chanW = m_CurrentPattern.m_fChannelWidth;
				if (slot.m_fThrowRadius > 0) rad = slot.m_fThrowRadius;
				if (slot.m_fTriggerHeight > 0) trigH = slot.m_fTriggerHeight;
				if (slot.m_fChannelWidth > 0) chanW = slot.m_fChannelWidth;
				
				float targetLaneX = m_fDefOriginX + (idx * gateOffset);
				float targetLimitY = m_fDefOriginY + (dir * rad);
				float distToLane = Math.AbsFloat(m_fClutchX - targetLaneX);
				if (distToLane <= (chanW * 0.5))
				{
					if (dir < 0) { if (m_fClutchY <= (targetLimitY + trigH)) { selectedGear = slot.m_iGearIndex; break; } }
					else { if (m_fClutchY >= (targetLimitY - trigH)) { selectedGear = slot.m_iGearIndex; break; } }
				}
			}
		}
		else
		{
			float thresTop = m_fDefOriginY - m_fDefRadius + 10.0;
			float thresBot = m_fDefOriginY + m_fDefRadius - 10.0;
			if (m_fClutchY < thresTop) 
			{
				if (m_fClutchX < m_fDefOriginX - 25.0) selectedGear = 1;
				else if (m_fClutchX > m_fDefOriginX + 25.0) selectedGear = 5;
				else selectedGear = 3;
			}
			else if (m_fClutchY > thresBot)
			{
				if (m_fClutchX < m_fDefOriginX - 25.0) selectedGear = 2;
				else if (m_fClutchX > m_fDefOriginX + 25.0) selectedGear = -1;
				else selectedGear = 4;
			}
		}
		
		if (selectedGear != m_iCurrentGear) m_iCurrentGear = selectedGear;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void Rpc_SetGear(int gearIndex)
	{
		ApplyGearToVehicle(gearIndex);
	}

	protected void ApplyGearToVehicle(int gearIndex)
	{
		if (!m_CarController) return;
		m_CarController.SetDrivingAssistanceMode(EVehicleDrivingAssistanceMode.NONE);
		VehicleWheeledSimulation simulation = m_CarController.GetWheeledSimulation();
		if (!simulation) return;
		
		int engineGearIndex = 0;
		if (gearIndex == -1) engineGearIndex = 0;
		else engineGearIndex = gearIndex + 1;

		simulation.SetGear(engineGearIndex);
	}

	protected bool IsProxy()
	{
		return (m_RplComponent && !m_RplComponent.IsOwner());
	}
	
	protected bool IsPlayerDriver()
	{
		if (!m_CompartmentAccess) return false;
		BaseCompartmentSlot slot = m_CompartmentAccess.GetCompartment();
		if (!slot) return false;
		if (!PilotCompartmentSlot.Cast(slot)) return false;
		return true;
	}
	
	protected float SnapToClosestChannel(float rawX, float offset)
	{
		float idx = Math.Round((rawX - m_fDefOriginX) / offset);
		return m_fDefOriginX + (idx * offset);
	}
}