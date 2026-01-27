// ------------------------------------------------------------------------------------------------
// DATA CLASSES
// ------------------------------------------------------------------------------------------------

// 1. The Container for a single vehicle pattern
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_sName")]
class SCR_GearboxPattern
{
	[Attribute("New Pattern", UIWidgets.EditBox)]
	string m_sName;
	
	[Attribute("", UIWidgets.EditBox, "Vehicle Prefab Names (Partial match allowed, e.g. 'S105')")]
	ref array<string> m_aVehicleFilters;

	[Attribute("", UIWidgets.Object)]
	ref array<ref SCR_GearSlot> m_aGearSlots;
	
	// --- VISUALS ---
	[Attribute("", UIWidgets.EditBox, "Image Name (Quad) inside the Imageset")]
	string m_sQuadName;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "OPTIONAL: Custom Imageset (Overrides Global Default)", "imageset")]
	ResourceName m_sCustomImageset;
	
	// --- FEEL SETTINGS ---
	[Attribute("1.0", UIWidgets.Slider, "Sensitivity Multiplier (0.5 = Heavy Mouse)", "0.1 5.0 0.1")]
	float m_fSensitivityMultiplier;
	
	[Attribute("76.0", UIWidgets.Slider, "Throw Radius (Vertical Length of throw)", "30.0 150.0 1.0")]
	float m_fThrowRadius;
	
	[Attribute("50.0", UIWidgets.Slider, "Gate Offset (Distance from Center to Left/Right lanes)", "10.0 200.0 1.0")]
	float m_fGateOffset;
	
	[Attribute("20.0", UIWidgets.Slider, "Channel Width (Vertical path width)", "5.0 100.0 1.0")]
	float m_fChannelWidth;
	
	[Attribute("20.0", UIWidgets.Slider, "Neutral Height (Horizontal band height)", "5.0 100.0 1.0")]
	float m_fNeutralHeight;
}

// 2. The Container for a single gear slot
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_iGearIndex")]
class SCR_GearSlot
{
	[Attribute("1", UIWidgets.EditBox, "Gear Index (-1=Rev, 0=Neutral, 1=1st...)")]
	int m_iGearIndex;
	
	[Attribute("-50.0 -80.0 0.0", UIWidgets.Coords, "Position (X, Y) relative to center")]
	vector m_vPosition;
	
	[Attribute("25.0", UIWidgets.Slider, "Hit Radius (Sweet spot size)")]
	float m_fRadius;
}

// 3. THE ROOT CONFIG (Your Master List)
[BaseContainerProps(configRoot: true), SCR_BaseContainerCustomTitleUIInfo("m_sConfigName")]
class SCR_ClutchConfig
{
	[Attribute("Global Clutch Config", UIWidgets.EditBox)]
	string m_sConfigName;

	[Attribute("", UIWidgets.Object, "List of all Shift Patterns")]
	ref array<ref SCR_GearboxPattern> m_aShiftPatterns;
}

// ------------------------------------------------------------------------------------------------
// NEW: VEHICLE COMPONENT (For Modders)
// ------------------------------------------------------------------------------------------------
// Modders attach this to their Vehicle Prefab to define custom shifting without touching your config.
[ComponentEditorProps(category: "GameScripted/Vehicle", description: "Defines Manual Shifting Pattern for this specific vehicle.")]
class SCR_ManualTransmissionComponentClass : ScriptComponentClass
{
}

class SCR_ManualTransmissionComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Object, "The Shift Pattern for THIS vehicle")]
	ref SCR_GearboxPattern m_VehiclePattern;
}

// ------------------------------------------------------------------------------------------------
// CONTROLLER CLASS
// ------------------------------------------------------------------------------------------------

modded class SCR_PlayerController : PlayerController
{
	// ------------------------------------------------------------------------------------------------
	// CONFIGURATION
	// ------------------------------------------------------------------------------------------------
	
	[Attribute("{E5CB8EC5EB63BC54}Configs/UI_ClutchDisplay.layout", UIWidgets.ResourceNamePicker, "The UI layout with the clutch circle")]
	protected ResourceName m_sClutchLayout;
	
	// IMAGESET
	[Attribute("", UIWidgets.ResourceNamePicker, "The .imageset file containing all patterns", "imageset")]
	protected ResourceName m_sClutchImageset;

	[Attribute("", UIWidgets.EditBox, "Default Quad Name (Fallback image name)")]
	protected string m_sDefaultQuadName;

	// GLOBAL SETTINGS
	[Attribute("1.0", UIWidgets.Slider, "Global Base Sensitivity", "0.1 20.0 0.1")]
	protected float m_fBaseSensitivity;
	
	[Attribute("0.4", UIWidgets.Slider, "Inactive Opacity (Fade)", "0.0 1.0 0.1")]
	protected float m_fInactiveOpacity;

	// LINK TO THE CONFIG FILE
	[Attribute("", UIWidgets.ResourceNamePicker, "The Global Config File (.conf)", "conf")]
	protected ResourceName m_sClutchConfigFile;

	// ------------------------------------------------------------------------------------------------
	// VARIABLES
	// ------------------------------------------------------------------------------------------------
	
	protected Widget m_wRoot;
	protected ImageWidget m_wClutchCircle;
	protected ImageWidget m_wPatternBackground;
	
	protected float m_fClutchX = 0.0;
	protected float m_fClutchY = 0.0;
	protected int m_iCurrentGear = 0; 
	protected bool m_bWasPressed = false;
	protected float m_fLastHorizontalPos = 0.0;
	
	// CACHED COMPONENTS
	protected SCR_CompartmentAccessComponent m_CompartmentAccess;
	protected RplComponent m_RplComponent;
	protected CarControllerComponent m_CarController;
	
	// LOADED CONFIG
	protected ref SCR_ClutchConfig m_ClutchConfigObject;
	protected ref SCR_GearboxPattern m_CurrentPattern;

	// DEFAULT FALLBACKS
	protected float m_fDefOriginX = -14.0;
	protected float m_fDefOriginY = -110.0;
	protected float m_fDefRadius = 76.0;
	protected float m_fDefGateOffset = 50.0;
	protected float m_fDefChanWidth = 20.0;
	protected float m_fDefNeutralHeight = 20.0;

	// ------------------------------------------------------------------------------------------------
	// INITIALIZATION
	// ------------------------------------------------------------------------------------------------

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		m_fClutchX = m_fDefOriginX;
		m_fClutchY = m_fDefOriginY;
		m_fLastHorizontalPos = m_fDefOriginX;
		
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		
		// LOAD CONFIG FILE
		if (m_sClutchConfigFile != "")
		{
			Resource resource = Resource.Load(m_sClutchConfigFile);
			if (resource && resource.IsValid())
			{
				m_ClutchConfigObject = SCR_ClutchConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
				if (m_ClutchConfigObject) Print("Clutch Mod: Loaded Config File Successfully.", LogLevel.NORMAL);
			}
		}
		
		HookEvents(GetControlledEntity());
	}
	
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		HookEvents(to);
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
			
			if (m_CurrentPattern) 
			{
				if (!m_wRoot) 
				{
					CreateClutchUI();
					SyncUIWithVehicle(); 
					
					if (m_CarController)
					{
						m_CarController.SetDrivingAssistanceMode(EVehicleDrivingAssistanceMode.NONE);
					}
				}
			}
			else
			{
				if (m_wRoot)
				{
					m_wRoot.RemoveFromHierarchy();
					m_wRoot = null;
				}
			}
		}
		else
		{
			if (m_wRoot)
			{
				m_wRoot.RemoveFromHierarchy();
				m_wRoot = null;
			}
		}
	}
	
	// --- UPDATED RESOLVER LOGIC ---
	protected void ResolveShiftPattern()
	{
		m_CurrentPattern = null;
		
		IEntity vehicle = GetVehicle();
		if (!vehicle) return;

		// 1. PRIORITY: Check for Modder Component attached to the Vehicle
		SCR_ManualTransmissionComponent modderComp = SCR_ManualTransmissionComponent.Cast(vehicle.FindComponent(SCR_ManualTransmissionComponent));
		if (modderComp && modderComp.m_VehiclePattern)
		{
			m_CurrentPattern = modderComp.m_VehiclePattern;
			Print(string.Format("Clutch Mod: Loaded Custom Pattern from Vehicle Component: %1", m_CurrentPattern.m_sName), LogLevel.NORMAL);
			return; // We found a local override, stop searching.
		}
		
		// 2. FALLBACK: Check Global Config (For Vanilla/Base Game Vehicles)
		if (!m_ClutchConfigObject) return;
		
		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (!prefabData) return;
		
		string prefabName = prefabData.GetPrefabName();
		
		if (m_ClutchConfigObject.m_aShiftPatterns)
		{
			foreach (SCR_GearboxPattern pattern : m_ClutchConfigObject.m_aShiftPatterns)
			{
				foreach (string filter : pattern.m_aVehicleFilters)
				{
					if (prefabName.Contains(filter))
					{
						m_CurrentPattern = pattern;
						return; 
					}
				}
			}
		}
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

		if (isPressed) 
		{
			if (!m_bWasPressed) SyncUIWithVehicle();
			
			m_wRoot.SetOpacity(1.0);
			inputManager.ActivateContext("Clutch_Context");
			UpdateClutchCircle(inputManager, timeSlice);
		}
		else
		{
			if (m_bWasPressed) m_fLastHorizontalPos = m_fClutchX;
			m_wRoot.SetOpacity(m_fInactiveOpacity);
		}
		
		m_bWasPressed = isPressed;
	}

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
				
				if (m_CurrentPattern.m_sCustomImageset != "")
				{
					targetImageset = m_CurrentPattern.m_sCustomImageset;
				}
			}
			
			if (m_wPatternBackground && targetImageset != "" && quadName != "")
			{
				m_wPatternBackground.LoadImageFromSet(0, targetImageset, quadName);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// PHYSICS & SYNC
	// ------------------------------------------------------------------------------------------------

	protected void SyncUIWithVehicle()
	{
		if (!m_CarController)
		{
			IEntity vehicle = GetVehicle();
			if (!vehicle) return;
			m_CarController = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		}
		if (!m_CarController) return;

		VehicleWheeledSimulation simulation = m_CarController.GetWheeledSimulation();
		if (!simulation) return;

		int engineGear = simulation.GetGear();
		if (engineGear == 0) m_iCurrentGear = -1;
		else m_iCurrentGear = engineGear - 1; 

		// DYNAMIC CONFIG VALUES
		float currentRadius = m_fDefRadius;
		float currentGateOffset = m_fDefGateOffset;
		
		if (m_CurrentPattern)
		{
			currentRadius = m_CurrentPattern.m_fThrowRadius;
			currentGateOffset = m_CurrentPattern.m_fGateOffset;
		}

		float targetX = m_fDefOriginX;
		float targetY = m_fDefOriginY;
		
		float leftX   = m_fDefOriginX - currentGateOffset;
		float rightX  = m_fDefOriginX + currentGateOffset;
		
		float topY    = m_fDefOriginY - currentRadius + 6.0; 
		float botY    = m_fDefOriginY + currentRadius - 6.0;

		if (m_CurrentPattern)
		{
			bool found = false;
			foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
			{
				if (slot.m_iGearIndex == m_iCurrentGear)
				{
					targetX = m_fDefOriginX + slot.m_vPosition[0];
					targetY = m_fDefOriginY + slot.m_vPosition[1];
					found = true;
					break;
				}
			}
			
			if (!found && m_iCurrentGear == 0)
			{
				targetX = SnapToChannel(m_fLastHorizontalPos, leftX, m_fDefOriginX, rightX);
				targetY = m_fDefOriginY;
			}
		}
		else
		{
			// Fallback
			if (m_iCurrentGear == 0)      
			{ 
				targetX = SnapToChannel(m_fLastHorizontalPos, leftX, m_fDefOriginX, rightX); 
				targetY = m_fDefOriginY; 
			} 
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

		// Default values
		float sensitivity = m_fBaseSensitivity;
		float gateOffset = m_fDefGateOffset;
		float chanWidth = m_fDefChanWidth;
		float neutHeight = m_fDefNeutralHeight;
		float throwRadius = m_fDefRadius;

		// Override from Config if available
		if (m_CurrentPattern)
		{
			sensitivity = m_fBaseSensitivity * m_CurrentPattern.m_fSensitivityMultiplier;
			gateOffset = m_CurrentPattern.m_fGateOffset;
			chanWidth = m_CurrentPattern.m_fChannelWidth;
			neutHeight = m_CurrentPattern.m_fNeutralHeight;
			throwRadius = m_CurrentPattern.m_fThrowRadius;
		}
		
		float chanHalf = chanWidth * 0.5;
		float neutHalf = neutHeight * 0.5;

		float mouseX = inputManager.GetActionValue("Clutch_X");
		float mouseY = inputManager.GetActionValue("Clutch_Y");
		
		float pendingX = m_fClutchX + (mouseX * sensitivity);
		float pendingY = m_fClutchY + (mouseY * sensitivity);

		float padding = 20.0;
		pendingX = Math.Clamp(pendingX, m_fDefOriginX - (gateOffset + 50.0), m_fDefOriginX + (gateOffset + 50.0));
		pendingY = Math.Clamp(pendingY, m_fDefOriginY - (throwRadius + padding), m_fDefOriginY + (throwRadius + padding));

		float xLeft   = m_fDefOriginX - gateOffset;
		float xCenter = m_fDefOriginX;
		float xRight  = m_fDefOriginX + gateOffset;
		
		float neutralTop    = m_fDefOriginY - neutHalf;
		float neutralBottom = m_fDefOriginY + neutHalf;

		// Resolve X
		bool inNeutralBand = false;
		if (pendingY > neutralTop)
		{
			if (pendingY < neutralBottom)
				inNeutralBand = true;
		}
		
		if (!inNeutralBand)
		{
			if (m_fClutchX < (xLeft + chanHalf + 5.0)) 
				pendingX = Math.Clamp(pendingX, xLeft - chanHalf, xLeft + chanHalf);
			else if (m_fClutchX > (xRight - chanHalf - 5.0)) 
				pendingX = Math.Clamp(pendingX, xRight - chanHalf, xRight + chanHalf);
			else
				pendingX = Math.Clamp(pendingX, xCenter - chanHalf, xCenter + chanHalf);
		}

		m_fClutchX = pendingX;

		// Resolve Y
		bool alignedLeft = (m_fClutchX >= (xLeft - chanHalf) && m_fClutchX <= (xLeft + chanHalf));
		bool alignedCenter = (m_fClutchX >= (xCenter - chanHalf) && m_fClutchX <= (xCenter + chanHalf));
		bool alignedRight = (m_fClutchX >= (xRight - chanHalf) && m_fClutchX <= (xRight + chanHalf));
		
		if (!alignedLeft && !alignedCenter && !alignedRight)
		{
			pendingY = Math.Clamp(pendingY, neutralTop, neutralBottom);
		}

		m_fClutchY = pendingY;
		
		FrameSlot.SetPos(m_wClutchCircle, m_fClutchX, m_fClutchY);
		
		CheckGearShift();
	}

	protected void CheckGearShift()
	{
		int selectedGear = 0; // Default Neutral

		if (m_CurrentPattern)
		{
			foreach (SCR_GearSlot slot : m_CurrentPattern.m_aGearSlots)
			{
				float slotWorldX = m_fDefOriginX + slot.m_vPosition[0];
				float slotWorldY = m_fDefOriginY + slot.m_vPosition[1];
				
				float dist = vector.Distance(Vector(m_fClutchX, m_fClutchY, 0), Vector(slotWorldX, slotWorldY, 0));
				
				if (dist <= slot.m_fRadius)
				{
					selectedGear = slot.m_iGearIndex;
					break;
				}
			}
		}
		else
		{
			selectedGear = GetGearFromPositionDefault(m_fClutchX, m_fClutchY);
		}
		
		if (selectedGear != m_iCurrentGear)
		{
			m_iCurrentGear = selectedGear;
			ApplyGearToVehicle(m_iCurrentGear);
			Rpc(Rpc_SetGear, m_iCurrentGear);
		}
	}

	protected int GetGearFromPositionDefault(float x, float y)
	{
		float thresTop = m_fDefOriginY - m_fDefRadius + 10.0;
		float thresBot = m_fDefOriginY + m_fDefRadius - 10.0;
		
		bool isTop = (y < thresTop);
		bool isBottom = (y > thresBot);
		
		if (!isTop && !isBottom) return 0;
		
		float midLeft  = m_fDefOriginX - 25.0;
		float midRight = m_fDefOriginX + 25.0;
		
		if (x < midLeft) 
		{
			if (isTop) return 1;
			else return 2;
		}
		else if (x > midRight) 
		{
			if (isTop) return 5;
			else return -1;
		}
		else 
		{
			if (isTop) return 3;
			else return 4;
		}
		return 0; // Fallback
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void Rpc_SetGear(int gearIndex)
	{
		ApplyGearToVehicle(gearIndex);
	}

	protected void ApplyGearToVehicle(int gearIndex)
	{
		if (!m_CarController)
		{
			IEntity player = GetControlledEntity();
			if (!player) return;
			
			if (!m_CompartmentAccess)
				m_CompartmentAccess = SCR_CompartmentAccessComponent.Cast(player.FindComponent(SCR_CompartmentAccessComponent));
				
			if (!m_CompartmentAccess) return;
			BaseCompartmentSlot slot = m_CompartmentAccess.GetCompartment();
			if (!slot) return;
			IEntity vehicle = slot.GetOwner();
			m_CarController = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		}
		
		if (!m_CarController) return;
		
		// ALWAYS FORCE MANUAL if we are in this function
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
	
	protected float SnapToChannel(float rawX, float left, float center, float right)
	{
		float distLeft = Math.AbsFloat(rawX - left);
		float distCenter = Math.AbsFloat(rawX - center);
		float distRight = Math.AbsFloat(rawX - right);
		
		if (distLeft < distCenter && distLeft < distRight) return left;
		if (distRight < distCenter && distRight < distLeft) return right;
		return center;
	}
}