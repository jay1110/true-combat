#include "PrecompCommon.h"
#include "NavStudioDiagnostics.h"
#include "Client.h"
#include "IGameManager.h"
#include "IGame.h"
#include "ScriptManager.h"
#include "WeaponDatabase.h"
#include "BotBaseStates.h"
#include "MovementCaps.h"
#include "gmBot.h"
#include "ScriptGoal.h"
#include "gmScriptGoal.h"
#include "../../../../../src/game/tce_nav_water.h"

//////////////////////////////////////////////////////////////////////////

Client::HoldButtons::HoldButtons()
{
	for(int i = 0; i < NumButtons; ++i)
		m_StopHoldTime[i] = 0;
}

Client::Client()
	: m_DesiredTeam		(RANDOM_TEAM_IF_NO_TEAM)
	, m_DesiredClass	(RANDOM_CLASS_IF_NO_CLASS)
	, m_SpawnTime(0)
	, m_RevivedTime(-9999)
	, m_StepHeight		(0.0f)
	, m_StateRoot		(NULL)
	, m_Position		(Vector3f::ZERO)
	, m_EyePosition		(Vector3f::ZERO)
	, m_MoveVector		(Vector3f::ZERO)
	, m_Velocity		(Vector3f::ZERO)
	, m_FacingVector	(Vector3f::UNIT_Y)
	, m_UpVector		(Vector3f::UNIT_Z)
	, m_RightVector		(Vector3f::UNIT_X)
	, m_WorldBounds		()
	, m_ButtonFlags		(0)
	, m_FieldOfView		(90.0f)
	, m_MaxViewDistance	(10000.0f)
	, m_RoleMask		(0)
	, m_Team			(0)
	, m_Class			(0)
	, m_GameID			(-1)
	, m_ScriptObject		(0)
	, m_CurrentTurnSpeed	(0.0f)
	, m_MaxTurnSpeed		(720.0f)
	, m_AimStiffness		(75.0f)
	, m_AimDamping			(10.0f)
	, m_AimTolerance		(48.0f)
	, m_ProfileType			(PROFILE_NONE)
{
#if defined(_DEBUG) && defined(FEATURE_NAVSTUDIO)
	m_DebugFlags.SetFlag(BOT_DEBUG_FPINFO);
#endif

	// Initialize default movement capability
	m_MovementCaps.SetFlag(Movement::MOVE_WALK, true);
	m_MovementCaps.SetFlag(Movement::MOVE_JUMP, true);
}

Client::~Client()
{
	gmMachine *pMachine = ScriptManager::GetInstance()->GetMachine(); 
	if(m_ScriptObject)
	{
		pMachine->RemoveCPPOwnedGMObject(m_ScriptObject);
		gmBot::NullifyObject(m_ScriptObject);
		m_ScriptObject = NULL;
	}
}

void Client::Update()
{
	Prof(ClientUpdate);

	// Set dirty flags to invalidate caches.
	m_InternalFlags.SetFlag(FL_DIRTYEYEPOS);

	{
		Prof(Interface_Functions);

		// update my locally properties with the one the game has for me.	
		EngineFuncs::EntityPosition(m_GameEntity, m_Position);
		EngineFuncs::EntityWorldOBB(m_GameEntity, m_WorldBounds);
		EngineFuncs::EntityGroundEntity(m_GameEntity, m_MoveEntity);

		// Update the bots orientation if we haven't turned this frame(script might be controlling)
		EngineFuncs::EntityOrientation(m_GameEntity, m_FacingVector, m_RightVector, m_UpVector);
		m_Orientation = Matrix3f(m_RightVector, m_FacingVector, m_UpVector, true);

		EngineFuncs::EntityVelocity(m_GameEntity, m_Velocity);

		Msg_PlayerMaxSpeed maxSpeed;
		if(InterfaceFuncs::GetMaxSpeed(m_GameEntity, maxSpeed))
		{
			m_MaxSpeed = maxSpeed.m_MaxSpeed;
		}

		// Update my health/armor
		InterfaceFuncs::GetHealthAndArmor(GetGameEntity(), m_HealthArmor);
		InterfaceFuncs::GetEntityFlags(m_GameEntity, m_EntityFlags);
		InterfaceFuncs::GetEntityPowerUps(m_GameEntity, m_EntityPowerUps);
	}

	if(CheckUserFlag(Client::FL_DISABLED))
	{
		// stop when disabled.
		m_MoveVector = Vector3f::ZERO;
	}
	else
	{
		CheckStuck();

		// Check for various events.
		CheckTeamEvent();
		CheckClassEvent();

		//////////////////////////////////////////////////////////////////////////
		// Process Sounds
		//const int numMsgs = g_SoundDepot.GetNumMessagesForSubscriber(m_SoundSubscriber);
		//if(numMsgs>0)
		//{
		//	Event_Sound *snds = (Event_Sound*)StackAlloc(sizeof(Event_Sound)*numMsgs);
		//	int n = g_SoundDepot.Collect(snds,numMsgs,m_SoundSubscriber);
		//	OBASSERT(n==numMsgs,"Problem getting events.");
		//	for(int i = 0; i < numMsgs; ++i)
		//	{
		//		//snds[i]
		//	}
		//}
		//////////////////////////////////////////////////////////////////////////

		// Check if we're dead or alive so we know what update function to call.
		if(m_StateRoot)
		{
			m_StateRoot->RootUpdate();
		}

		// Purge expired blackboard records.
		GetBB().PurgeExpiredRecords();

		// Set any buttons that should be held down.
		if(!m_ButtonFlags.CheckFlag(BOT_BUTTON_RESPAWN))
		{
			for(int i = 0; i < HoldButtons::NumButtons; ++i)
			{
				// If it's not already set.
				if(!IsButtonDown(i))
				{
					// Should this button be held down?
					if(m_HoldButtons.m_StopHoldTime[i] > (obuint32)IGame::GetTime())
					{
						PressButton(i);
					}
				}		
			}
		}

		if (m_ButtonFlags.CheckFlag(BOT_BUTTON_JUMP) && !m_StuckExpanded && !UseValidatedWalkEntries())
		{
			m_StuckExpanded = true;
			m_StuckBounds.ExpandAxis(0, 20);
			m_StuckBounds.ExpandAxis(1, 20);
			m_StuckBounds.ExpandAxis(2, 40); //must be higher than JUMP height
		}
	}	

	// Observe the completed steering/state result, never an earlier desired
	// vector. Only recently crossed TCE door actions enable this bounded log.
	if(UseValidatedWalkEntries() && GetFollowPath()) GetFollowPath()->TraceDoorMotion();
	// Update my input with the engine
	m_ClientInput.m_MoveDir[0]		= m_MoveVector.x;
	m_ClientInput.m_MoveDir[1]		= m_MoveVector.y;
	m_ClientInput.m_MoveDir[2]		= m_MoveVector.z;
	m_ClientInput.m_Facing[0]		= m_FacingVector.x;
	m_ClientInput.m_Facing[1]		= m_FacingVector.y;
	m_ClientInput.m_Facing[2]		= m_FacingVector.z;
	m_ClientInput.m_ButtonFlags		= m_ButtonFlags;
	m_ClientInput.m_CurrentWeapon	= GetWeaponSystem()->GetDesiredWeaponID();
	{
		Prof(UpdateBotInput);
		UpdateBotInput();
	}

#ifdef _DEBUG
	OBASSERT(m_FacingVector != Vector3f::ZERO, "Zero Facing Vector");	
#endif

	// Zero the button flags here, if we do it before the UpdateBotInput, any script
	// controlling the bot will get overridden.
	m_ButtonFlags.ClearAll();
}

void Client::UpdateBotInput()
{
	g_EngineFuncs->UpdateBotInput(m_GameID, m_ClientInput);
}


#ifdef ENABLE_REMOTE_DEBUGGING
void Client::InternalSyncEntity( EntitySnapShot & snapShot, RemoteLib::DataBuffer & db ) {
	snapShot.Sync( "fov", GetFieldOfView(), db );
}
#endif

const char *Client::GetName(bool _clean) const
{
	return g_EngineFuncs->GetEntityName(GetGameEntity());
}

Vector3f Client::GetEyePosition()
{
	if(m_InternalFlags.CheckFlag(FL_DIRTYEYEPOS))
	{
		EngineFuncs::EntityEyePosition(m_GameEntity, m_EyePosition);
		m_InternalFlags.ClearFlag(FL_DIRTYEYEPOS);
	}
	return m_EyePosition;
}

gmUserObject *Client::GetScriptObject()
{
	return m_ScriptObject; 
}

gmVariable Client::GetScriptVariable()
{
	return m_ScriptObject?gmVariable(m_ScriptObject):gmVariable::s_null;
}

void Client::ProcessEvent(const MessageHelper &_message, CallbackParameters &_cb)
{
	switch(_message.GetMessageId())
	{
		HANDLER(GAME_CLIENTDISCONNECTED)
		{
			_cb.CallScript(true);
			const Event_SystemClientDisConnected *m = _message.Get<Event_SystemClientDisConnected>();
			GetBB().RemoveBBRecordByPoster(m->m_GameId, bbk_All);
			GetBB().RemoveBBRecordByTarget(m->m_GameId, bbk_All);
			break;
		}
		HANDLER(MESSAGE_SPAWN)
		{
			// Clear the goal list so the bot can re-evaluate the goals
			_cb.CallScript();
			if(UseValidatedWalkEntries())
			{
				GetFollowPath()->Stop(true);
				GetFollowPath()->ResetTceDropTrafficLease();
				GetFollowPath()->ResetTceOrdinaryLadderContact();
			}
			ResetStuckTime();
			ReleaseAllHeldButtons();
			break;
		}		
		HANDLER(MESSAGE_DEATH)
		{
			State *state = GetHighLevel()->GetActiveState();
			if(state)
			{
				MapGoal *g = state->GetMapGoalPtr();
				if(g)
				{
					m_PreviousGoal = g;
					m_PreviousGoalTime = IGame::GetTime();
				}
			}

			const Event_Death *m = _message.Get<Event_Death>();
			_cb.CallScript();
			_cb.AddEntity("inflictor", m->m_WhoKilledMe);				
			_cb.AddString("meansofdeath", m->m_MeansOfDeath);
			break;
		}
		HANDLER(MESSAGE_HEALED)
		{
			const Event_Healed *m = _message.Get<Event_Healed>();
			_cb.CallScript();
			_cb.AddEntity("who", m->m_WhoHealedMe);
			break;
		}
		HANDLER(MESSAGE_REVIVED)
		{
			m_RevivedTime = IGame::GetTime();
			const Event_Revived *m = _message.Get<Event_Revived>();
			_cb.CallScript();
			_cb.AddEntity("who", m->m_WhoRevivedMe);
			break;
		}
		HANDLER(MESSAGE_KILLEDSOMEONE)
		{
			const Event_KilledSomeone *m = _message.Get<Event_KilledSomeone>();
			_cb.CallScript();
			_cb.AddEntity("victim", m->m_WhoIKilled);
			_cb.AddString("meansofdeath", m->m_MeansOfDeath);
			break;
		}	
		HANDLER(MESSAGE_CHANGETEAM)
		{
			const Event_ChangeTeam *m = _message.Get<Event_ChangeTeam>();
			_cb.CallScript();
			_cb.AddInt("newteam", m->m_NewTeam);
			m_Team = m->m_NewTeam;
			break;
		}
		HANDLER(MESSAGE_CHANGECLASS)
		{
			const Event_ChangeClass *m = _message.Get<Event_ChangeClass>();
			_cb.CallScript();
			_cb.AddInt("classId", m->m_NewClass);

			if(GetProfileType() != Client::PROFILE_CUSTOM)
				LoadProfile(Client::PROFILE_CLASS);
			break;
		}
		HANDLER(MESSAGE_SPECTATED)
		{
			//const Event_Spectated *m = _message.Get<Event_Spectated>();
			if(IsDebugEnabled(BOT_DEBUG_FPINFO))
			{
				StringStr strOutString;

				State *ActiveState = GetHighLevel()->GetActiveState();
				if(ActiveState)
				{
					while(ActiveState->GetActiveState())
						ActiveState = ActiveState->GetActiveState();

					std::streampos pos = strOutString.tellp();
					ActiveState->GetDebugString(strOutString);
					if(pos == strOutString.tellp()) strOutString << ActiveState->GetName();
					strOutString << std::endl;
				}
				else
				{
					strOutString << "No Goal" << std::endl;
				}

				const MemoryRecord *pTargetRecord = GetTargetingSystem()->GetCurrentTargetRecord();
				if(pTargetRecord)
				{
					//strOutString << "Target: " << 
					//	pTargetRecord->m_TargetInfo.m_DistanceTo << 
					//	" units away." << std::endl;
					strOutString  
                               << "Target: "  
                               << EngineFuncs::EntityName(pTargetRecord->GetEntity(),"<unknown>") 
                               << " " 
                               << (int)pTargetRecord->m_TargetInfo.m_DistanceTo  
                               << " units away." 
                               << std::endl;
				}
				else
					strOutString << "No Target" << std::endl;

				GetWeaponSystem()->GetSpectateMessage(strOutString);
				Utils::PrintText(Vector3f::ZERO, COLOR::WHITE, IGame::GetDeltaTimeSecs()*2.f, strOutString.str().c_str());
			}
			break;
		}
		HANDLER(MESSAGE_SCRIPTMSG)
		{
			const Event_ScriptMessage *m = _message.Get<Event_ScriptMessage>();
			_cb.CallScript();
			_cb.AddString("msg", m->m_MessageName);
			_cb.AddString("data1", m->m_MessageData1);
			_cb.AddString("data2", m->m_MessageData2);
			_cb.AddString("data3", m->m_MessageData3);
			break;
		}
		HANDLER(MESSAGE_PROXIMITY_TRIGGER)
		{
			const AiState::Event_ProximityTrigger *m = _message.Get<AiState::Event_ProximityTrigger>();
			_cb.CallScript();
			_cb.AddString("owner", Utils::HashToString(m->m_OwnerState).c_str());
			_cb.AddEntity("ent",m->m_Entity);
			_cb.AddVector("position",m->m_Position);
			break;
		}
		HANDLER(MESSAGE_ADDWEAPON)
		{
			const Event_AddWeapon *m = _message.Get<Event_AddWeapon>();
			int weaponId = IGameManager::GetInstance()->GetGame()->ConvertWeaponId(m->m_WeaponId);
			if(!GetWeaponSystem()->HasWeapon(weaponId))
			{
				// Add weapons according to the event.
				if(GetWeaponSystem()->AddWeaponToInventory(weaponId))
				{
					_cb.CallScript();
					_cb.AddInt("weapon", weaponId);
				}
			}
			break;
		}
		HANDLER(MESSAGE_REMOVEWEAPON)
		{
			const Event_RemoveWeapon *m = _message.Get<Event_RemoveWeapon>();
			if(GetWeaponSystem()->HasWeapon(m->m_WeaponId))
			{
				_cb.CallScript();
				_cb.AddInt("weapon", m->m_WeaponId);
				GetWeaponSystem()->RemoveWeapon(m->m_WeaponId);
			}
			break;
		}
		HANDLER(MESSAGE_RESETWEAPONS)
		{
			_cb.CallScript();
			GetWeaponSystem()->ClearWeapons();			
			break;
		}
		HANDLER(MESSAGE_REFRESHALLWEAPONS)
		{
			GetWeaponSystem()->RefreshAllWeapons();
			break;
		}
		HANDLER(MESSAGE_REFRESHWEAPON)
		{
			const Event_RefreshWeapon *m = _message.Get<Event_RefreshWeapon>();
			GetWeaponSystem()->RefreshWeapon(m->m_WeaponId);

			_cb.CallScript();
			_cb.AddInt("weapon", m->m_WeaponId);
			break;
		}
		HANDLER(MESSAGE_ENT_ENTER_RADIUS)
		{
			const Event_EntEnterRadius *m = _message.Get<Event_EntEnterRadius>();
			//_cb.CallScript();
			_cb.AddEntity("ent", m->m_Entity);
			break;
		}
		HANDLER(MESSAGE_ENT_LEAVE_RADIUS)
		{
			const Event_EntLeaveRadius *m = _message.Get<Event_EntLeaveRadius>();
			//_cb.CallScript();
			_cb.AddEntity("ent", m->m_Entity);
			break;
		}
		HANDLER(ACTION_WEAPON_CHANGE)
		{
			const Event_WeaponChanged *m = _message.Get<Event_WeaponChanged>();
			if(GetWeaponSystem()->HasWeapon(m->m_WeaponId))
			{
				_cb.CallScript();
				_cb.AddInt("weapon", m->m_WeaponId);

				// Special case signal.
				GetStateRoot()->SignalThreads(gmVariable(Utils::MakeId32((obint16)ACTION_WEAPON_CHANGE, (obint16)m->m_WeaponId)));
			}
			break;
		}
		HANDLER(ACTION_WEAPON_FIRE)
		{
			const Event_WeaponFire *m = _message.Get<Event_WeaponFire>();
			_cb.CallScript();
			_cb.AddInt("weapon", m->m_WeaponId);
			_cb.AddEntity("projectile", m->m_Projectile);

			// Shot fired callback.
			WeaponPtr curWpn = GetWeaponSystem()->GetCurrentWeapon();
			if(curWpn && curWpn->GetWeaponID() == m->m_WeaponId)
				curWpn->ShotFired(m->m_FireMode, m->m_Projectile);

			// Special case signal.
			GetStateRoot()->SignalThreads(gmVariable(Utils::MakeId32((obint16)ACTION_WEAPON_FIRE, (obint16)m->m_WeaponId)));
			break;
		}
		HANDLER(PERCEPT_FEEL_PLAYER_USE)
		{
			const Event_PlayerUsed *m = _message.Get<Event_PlayerUsed>();
			_cb.CallScript();
			_cb.AddEntity("toucher", m->m_WhoDidIt);
			break;
		}
		HANDLER(PERCEPT_FEEL_PAIN)
		{
			const Event_TakeDamage *m = _message.Get<Event_TakeDamage>();
			_cb.CallScript();
			_cb.AddEntity("inflictor", m->m_Inflictor);

			_cb.AddInt("previoushealth", GetCurrentHealth());
			InterfaceFuncs::GetHealthAndArmor(GetGameEntity(), m_HealthArmor);
			_cb.AddInt("currenthealth", GetCurrentHealth());

			if(m->m_Inflictor.IsValid())
			{
				GetSensoryMemory()->UpdateWithTouchSource(m->m_Inflictor);
			}
			break;
		}	
		HANDLER(PERCEPT_HEAR_GLOBALVOICEMACRO)
		HANDLER(PERCEPT_HEAR_TEAMVOICEMACRO)
		HANDLER(PERCEPT_HEAR_PRIVATEVOICEMACRO)
		{
			const Event_VoiceMacro *m = _message.Get<Event_VoiceMacro>();
			if(m->m_WhoSaidIt != GetGameEntity()) // Ignore messages from myself.
			{
				_cb.CallScript();

				int macroId = HandleVoiceMacroEvent(_message);

				_cb.AddEntity("who", m->m_WhoSaidIt);
				_cb.AddInt("macro", macroId);

				// signal any pending threads
				gmVariable s(Utils::MakeId32((obint16)PERCEPT_HEAR_VOICEMACRO, (obint16)macroId));
				GetStateRoot()->SignalThreads(s);
			}
			else
				_cb.DontPropogateEvent();
			break;
		}
		HANDLER(PERCEPT_HEAR_GLOBALCHATMSG)
		HANDLER(PERCEPT_HEAR_TEAMCHATMSG)	
		HANDLER(PERCEPT_HEAR_PRIVCHATMSG)
		HANDLER(PERCEPT_HEAR_GROUPCHATMSG)
		{
			const Event_ChatMessage *m = _message.Get<Event_ChatMessage>();
			if(m->m_WhoSaidIt != GetGameEntity()) // Ignore messages from myself.
			{
				_cb.CallScript();
				_cb.AddEntity("who", m->m_WhoSaidIt);
				_cb.AddString("msg", m->m_Message);
			}
			else
				_cb.DontPropogateEvent();
			break;
		}
		HANDLER(PERCEPT_HEAR_SOUND)
		{
			const Event_Sound *m = _message.Get<Event_Sound>();
			if(m->m_Source != GetGameEntity())
			{
				_cb.CallScript();
				_cb.AddEntity("source", m->m_Source);
				_cb.AddVector("origin", m->m_Origin[0], m->m_Origin[1], m->m_Origin[2]);
				_cb.AddInt("soundId", m->m_SoundType);
				_cb.AddString("soundName", m->m_SoundName);
				GetSensoryMemory()->UpdateWithSoundSource(m);
			}
			break;
		}
		HANDLER(PERCEPT_SENSE_ENTITY)
		{
			const Event_EntitySensed *m = _message.Get<Event_EntitySensed>();
			_cb.CallScript();
			_cb.AddInt("sensedclass", m->m_EntityClass);
			_cb.AddEntity("sensedentity", m->m_Entity);
			break;
		}
		default:
			break;
	}
}

void Client::Init(int _gameid)
{
	// Set the game id, which is the index into the m_Clients array.
	m_GameID = _gameid;
	m_GameEntity = g_EngineFuncs->EntityFromID(m_GameID);

	//m_SoundSubscriber = g_SoundDepot.Subscribe("Client");

	// mark name as taken
	if(const char *pName = g_EngineFuncs->GetEntityName(GetGameEntity()))
		m_NameReference = NameManager::GetInstance()->GetName(pName);

	// Only create these if they aren't already defined, 
	// the mod may override them with derived classes.

	// Add this bot to the global script table.
	m_ScriptObject = ScriptManager::GetInstance()->AddBotToGlobalTable(this);

	gmMachine *pMachine = ScriptManager::GetInstance()->GetMachine();

	InitBehaviorTree();
	InitScriptGoals();

	LoadProfile(PROFILE_CUSTOM);

	//////////////////////////////////////////////////////////////////////////	
	// Call any map callbacks.
	gmCall call;
	if(call.BeginGlobalFunction(pMachine, "OnBotJoin", gmVariable::s_null, true))
	{
		OBASSERT(m_ScriptObject, "No Script object.");
		call.AddParamUser(m_ScriptObject);
		call.End();
	}
	//////////////////////////////////////////////////////////////////////////
}

void Client::Shutdown()
{
	IGameManager::GetInstance()->SyncRemoteDelete( GetGameEntity().AsInt() );

	//////////////////////////////////////////////////////////////////////////	
	// Call any map callbacks.
	gmMachine *pMachine = ScriptManager::GetInstance()->GetMachine();
	gmCall call;
	if(call.BeginGlobalFunction(pMachine, "OnBotLeave", gmVariable::s_null, true))
	{
		OBASSERT(m_ScriptObject, "No Script object.");
		call.AddParamUser(m_ScriptObject);
		call.End();
	}

	if(m_StateRoot)
	{
		m_StateRoot->ExitAll();
	}
	OB_DELETE(m_StateRoot);

	ScriptManager::GetInstance()->RemoveFromGlobalTable(this);
}

void Client::CheckStuck()
{
	if(UseValidatedWalkEntries() && HasEntityFlag(ENT_FLAG_ONLADDER) &&
		m_StuckBounds.m_Maxs[2] - m_StuckBounds.m_Mins[2] > 16.f)
	{
		m_StuckBounds.Set(Vector3f(-8,-8,-8), Vector3f(8,8,8));
		m_StuckBounds.SetCenter(GetPosition());
		ResetStuckTime();
	}
	if(m_StuckBounds.Contains(GetPosition()))
	{
		m_StuckTime += IGame::GetDeltaTime();
	}
	else
	{
		ResetStuckTime();
		m_StuckExpanded = false;
		if(UseValidatedWalkEntries())
		{
			// Slow turns/crouch movement are progress. Repeated vertical hops at
			// the same obstruction are not; actual ladder movement is.
			const float z = HasEntityFlag(ENT_FLAG_ONLADDER) ? 8.f : 128.f;
			m_StuckBounds.Set(Vector3f(-8.f, -8.f, -z), Vector3f(8.f, 8.f, z));
		}
		else
			m_StuckBounds.Set(Vector3f(-32.f, -32.f, -32.f), Vector3f(32.f, 32.f, 32.f));
		m_StuckBounds.SetCenter(GetPosition());
	}

	/*if(m_Velocity.SquaredLength() < (m_MinSpeed*m_MinSpeed))
	return true;
	else
	return false;*/
}

bool Client::GetNavigationHull(AABB &_bounds, NavFlags _flags) const
{
	return EngineFuncs::EntityLocalAABB(GetGameEntity(), _bounds);
}

static bool NavigationGroundContentsSafe(const Vector3f &_feet, bool _allowWading, float _middleHeight)
{
	const int contents = g_EngineFuncs->GetPointContents(_feet);
	if(contents & (CONT_SLIME | CONT_LAVA)) return false;
	if(contents & CONT_WATER)
	{
		// Match PM_SetWaterLevel's real posture-dependent middle sample.
		// Other games retain their prior no-wading policy.
		if(!_allowWading || _middleHeight <= 0.f || (g_EngineFuncs->GetPointContents(_feet.AddZ(_middleHeight)) &
			(CONT_WATER | CONT_SLIME | CONT_LAVA))) return false;
	}
	return true;
}

static bool NavigationGroundSurfaceSafe(float _normalZ, int _surface, bool _allowFlatSlick)
{
	// Match native NG_OrdinarySurface: a horizontal slick floor still supports
	// PM_WalkMove. A sloped slick surface needs a separate sliding recipe.
	return _normalZ >= .7f && (!(_surface & SURFACE_SLICK) ||
		(_allowFlatSlick && _normalZ >= .9999f));
}

bool Client::CanWalkTo(const Vector3f &_destination, NavFlags _flags) const
{
	return TraceWalkTo(_destination, _flags, 0.f, NULL);
}

bool Client::ResolveWalkDestination(const Vector3f &_destination, float _radius,
	NavFlags _flags, Vector3f &_supported) const
{
	_supported = _destination;
	if(!UseValidatedWalkEntries()) return CanWalkTo(_destination, _flags);
	// The existing walk remains limited to 512 XY / 128 Z and 128 steps.
	// Invalid/unbounded caller radii cannot turn this into a floor search.
	if(!(_radius >= 0.f && _radius <= 100000.f)) return false;
	if(_radius == 0.f) return CanWalkTo(_destination, _flags);
	// Only the caller's appended final destination uses this volume contract.
	// A grounded actor already inside its original 3-D sphere need not walk
	// through an obstacle to the centre. Its current full body, support, slope
	// and water clearance must still pass the same fresh native walk proof.
	const Vector3f position = GetPosition();
	if(HasEntityFlag(ENT_FLAG_ONGROUND) &&
		(position-_destination).SquaredLength() <= _radius*_radius)
	{
		AABB hull;
		if(!GetNavigationHull(hull, _flags)) return false;
		int enlarged;
		if(hull.m_Mins[2] == -30.f) enlarged = 1;
		else if(hull.m_Mins[2] == -24.f) enlarged = 0;
		else return false;
		const float middle = TCE_NavWaterMiddleSample(enlarged,
			hull.m_Maxs[2]-hull.m_Mins[2] <= (enlarged ? 61.f : 49.f));
		const int mask = TR_MASK_SOLID | TR_MASK_PLAYERCLIP | TR_MASK_NAVCAPSULE;
		obTraceResult clearance, ground;
		// A stationary NAVCAPSULE query requires two unobstructed +/- .01u
		// sweeps. At PMove's .125u resting epsilon its downward half properly
		// hits the floor. Here prove clearance and SUPPORT separately instead:
		// both nonzero sweeps include the exact, unmodified current full hull.
		if(!EngineFuncs::TraceLine(clearance,position,position.AddZ(.01f),&hull,
			mask,GetGameID(),False) || clearance.m_StartSolid || clearance.m_Fraction < 1.f ||
			!EngineFuncs::TraceLine(ground,position,position.AddZ(-2.25f),&hull,
			mask,GetGameID(),False) || ground.m_StartSolid || ground.m_Fraction >= 1.f ||
			!NavigationGroundSurfaceSafe(ground.m_Normal[2],ground.m_Surface,true) ||
			Mathf::FAbs(ground.m_Endpos[2]-position.z) > 2.f ||
			!NavigationGroundContentsSafe(Vector3f(ground.m_Endpos).AddZ(hull.m_Mins[2]),true,middle)) return false;
		_supported = position;
		return true;
	}
	return TraceWalkTo(_destination, _flags, _radius, &_supported);
}

bool Client::TraceWalkTo(const Vector3f &_destination, NavFlags _flags,
	float _arrivalRadius, Vector3f *_supported) const
{
#define NAV_WALK_REJECT() do { if(IsDebugEnabled(BOT_DEBUG_PLANNER)) NAVSTUDIO_DIAGNOSTIC(va("TCE2WALK_REJECT bot=%d line=%d from=(%.3f %.3f %.3f) to=(%.3f %.3f %.3f)", GetGameID(), __LINE__, GetPosition().x,GetPosition().y,GetPosition().z,_destination.x,_destination.y,_destination.z)); return false; } while(0)
	// A bounded native step-and-ground walk. Player traffic is temporary and
	// deliberately excluded from this static entry/corner geometry decision.
	AABB hull;
	if(!GetNavigationHull(hull, _flags)) NAV_WALK_REJECT();
	float waterMiddleHeight=0.f;
	if(UseValidatedWalkEntries())
	{
		int enlarged;
		if(hull.m_Mins[2]==-30.f) enlarged=1;
		else if(hull.m_Mins[2]==-24.f) enlarged=0;
		else NAV_WALK_REJECT();
		const float height=hull.m_Maxs[2]-hull.m_Mins[2];
		waterMiddleHeight=TCE_NavWaterMiddleSample(enlarged,height<=(enlarged?61.f:49.f));
	}
	Vector3f position = GetPosition();
	Vector3f delta = _destination - position;
	const float horizontal = Mathf::Sqrt(delta.x*delta.x + delta.y*delta.y);
	if(!(horizontal <= 512.f) || !(Mathf::FAbs(delta.z) <= 128.f)) NAV_WALK_REJECT();
	const int steps = MaxT(1, (int)ceilf(delta.Length() / 8.f));
	if(steps > 128) NAV_WALK_REJECT();
	int tangentCorrections = 0;
	// ET_Client::m_StepHeight is an 8-unit early-jump heuristic, not PMove's
	// actual 18-unit step. TC's generated walk proof uses the latter.
	const float stepHeight = UseValidatedWalkEntries() ? 18.25f : GetStepHeight() + 0.25f;
	const int mask = TR_MASK_SOLID | TR_MASK_PLAYERCLIP;
	const int bodyMask = mask | (UseValidatedWalkEntries() ? TR_MASK_NAVCAPSULE : 0);
	if(UseValidatedWalkEntries())
	{
		// Follow PMove's body, not a separate flat foot box: on patch slopes
		// the capsule can rest above that box's floor. Prove each short move
		// and step with the complete hull, and require support after every move.
		for(int i=1; i<=steps; ++i)
		{
			const float fraction=(float)i/steps;
			Vector3f target(GetPosition().x+delta.x*fraction,
				GetPosition().y+delta.y*fraction,position.z);
			const auto capsuleStep = [&](Vector3f &supported) -> bool
			{
				obTraceResult horizontalTrace, ground;
				if(!EngineFuncs::TraceLine(horizontalTrace,position,target,&hull,bodyMask,GetGameID(),False) || horizontalTrace.m_StartSolid) return false;
				Vector3f above=target;
				if(horizontalTrace.m_Fraction<1.f)
				{
					const Vector3f raised=position.AddZ(18.f);
					above=target.AddZ(18.f);
					obTraceResult up;
					if(!EngineFuncs::TraceLine(up,position,raised,&hull,bodyMask,GetGameID(),False) || up.m_StartSolid || up.m_Fraction<1.f) return false;
					if(!EngineFuncs::TraceLine(horizontalTrace,raised,above,&hull,bodyMask,GetGameID(),False) || horizontalTrace.m_StartSolid || horizontalTrace.m_Fraction<1.f) return false;
				}
				if(!EngineFuncs::TraceLine(ground,above,target.AddZ(-18.5f),&hull,bodyMask,GetGameID(),False) || ground.m_StartSolid || ground.m_Fraction>=1.f || !NavigationGroundSurfaceSafe(ground.m_Normal[2],ground.m_Surface,true)) return false;
				supported=Vector3f(ground.m_Endpos)+Vector3f(0,0,.125f);
				return supported.z-position.z<=18.25f && position.z-supported.z<=18.5f &&
					NavigationGroundContentsSafe(supported.AddZ(hull.m_Mins[2]),true,waterMiddleHeight);
			};
			const auto groundedStep = [&](Vector3f &supported) -> bool
			{
				// A small slope does not require lifting the body a full 18u:
				// that artificial lift can strike a ceiling above a clear walk.
				// Match native NG_Ground's height proposal, but never accept its
				// flat foot box as support for the actual capsule across an edge.
				const Vector3f horizontalDelta=target-position;
				if(horizontalDelta.x*horizontalDelta.x+horizontalDelta.y*horizontalDelta.y<=.000001f) return false;
				AABB feet=hull;feet.m_Maxs[2]=feet.m_Mins[2]+.25f;
				obTraceResult foot,move,ground,clearance;
				if(!EngineFuncs::TraceLine(foot,target.AddZ(18.f),target.AddZ(-18.5f),&feet,mask,GetGameID(),False) ||
					foot.m_StartSolid || foot.m_Fraction>=1.f || !NavigationGroundSurfaceSafe(foot.m_Normal[2],foot.m_Surface,true)) return false;
				const Vector3f anchor=Vector3f(foot.m_Endpos).AddZ(.125f);
				if(anchor.z-position.z>18.25f || position.z-anchor.z>18.5f ||
					!EngineFuncs::TraceLine(move,position,anchor,&hull,bodyMask,GetGameID(),False) ||
					move.m_StartSolid || move.m_Fraction<1.f) return false;
				if(!EngineFuncs::TraceLine(ground,anchor,anchor.AddZ(-18.5f),&hull,bodyMask,GetGameID(),False) ||
					ground.m_StartSolid || ground.m_Fraction>=1.f ||
					!NavigationGroundSurfaceSafe(ground.m_Normal[2],ground.m_Surface,true)) return false;
				supported=Vector3f(ground.m_Endpos).AddZ(.125f);
				if(supported.z-position.z>18.25f || position.z-supported.z>18.5f ||
					!NavigationGroundContentsSafe(supported.AddZ(hull.m_Mins[2]),true,waterMiddleHeight)) return false;
				// Check the small final separation with the same full body too.
				return EngineFuncs::TraceLine(clearance,Vector3f(ground.m_Endpos),supported,&hull,bodyMask,GetGameID(),False) &&
					!clearance.m_StartSolid && clearance.m_Fraction>=1.f;
			};
			Vector3f supported;
			if(!capsuleStep(supported))
			{
				if(!groundedStep(supported)) NAV_WALK_REJECT();
				if(IsDebugEnabled(BOT_DEBUG_PLANNER)) NAVSTUDIO_DIAGNOSTIC(va("TCE2WALK_GROUNDFIRST bot=%d step=%d/%d from=(%.3f %.3f %.3f) supported=(%.3f %.3f %.3f)",
					GetGameID(),i,steps,position.x,position.y,position.z,supported.x,supported.y,supported.z));
			}
			position=supported;
		}
		if(IsDebugEnabled(BOT_DEBUG_PLANNER)) NAVSTUDIO_DIAGNOSTIC(va("TCE2WALK_CAPSULE bot=%d endz=%.3f destinationz=%.3f",GetGameID(),position.z,_destination.z));
		if(_supported)
		{
			// Every preceding 8u move already proved body clearance, support,
			// slope and water. This changes only the final goal-volume test,
			// never the walk proof or an authored waypoint/action coordinate.
			if((position-_destination).SquaredLength() > _arrivalRadius*_arrivalRadius) NAV_WALK_REJECT();
			*_supported = position;
			return true;
		}
		if(Mathf::FAbs(position.z-_destination.z)<=2.f) return true;
		// Exported anchors use flat-foot support. Accept its small slope offset
		// only when a fresh box support query reproduces that exact anchor;
		// the body route above is still required and cannot cross a gap.
		if(Mathf::FAbs(position.z-_destination.z)>stepHeight) NAV_WALK_REJECT();
		AABB supportBox=hull;
		supportBox.m_Maxs[2]=supportBox.m_Mins[2]+.25f;
		obTraceResult anchor;
		return EngineFuncs::TraceLine(anchor,position.AddZ(18.f),position.AddZ(-18.5f),
			&supportBox,mask,GetGameID(),False) && !anchor.m_StartSolid && anchor.m_Fraction<1.f &&
			NavigationGroundSurfaceSafe(anchor.m_Normal[2],anchor.m_Surface,true) &&
			Mathf::FAbs(anchor.m_Endpos[2]+.125f-_destination.z)<=2.f;
	}
	AABB feet = hull;
	feet.m_Maxs[2] = feet.m_Mins[2] + 0.25f;
	for(int i = 1; i <= steps; ++i)
	{
		const float fraction = (float)i / steps;
		Vector3f target(GetPosition().x + delta.x*fraction,
			GetPosition().y + delta.y*fraction, position.z);
		// The thin support box must be independent of headroom, exactly as in
		// NG_Ground: raising the whole player here rejects legal low doorsteps.
		obTraceResult ground;
		if(!EngineFuncs::TraceLine(ground, target.AddZ(18.f), target.AddZ(-18.5f),
			&feet, mask, GetGameID(), False) || ground.m_StartSolid || ground.m_Fraction >= 1.f ||
			!NavigationGroundSurfaceSafe(ground.m_Normal[2],ground.m_Surface,UseValidatedWalkEntries())) {
			if(IsDebugEnabled(BOT_DEBUG_PLANNER)) NAVSTUDIO_DIAGNOSTIC(va("TCE2WALK_GROUND bot=%d step=%d solid=%d frac=%.5f normal=%.4f targetz=%.4f endz=%.4f mins=%.2f maxs=%.2f", GetGameID(),i,(int)ground.m_StartSolid,ground.m_Fraction,ground.m_Normal[2],target.z,ground.m_Endpos[2],hull.m_Mins[2],hull.m_Maxs[2]));
			NAV_WALK_REJECT();
		}
		target = Vector3f(ground.m_Endpos) + Vector3f(0,0,0.125f);
		if(Mathf::FAbs(target.z-position.z) > stepHeight) NAV_WALK_REJECT();
		const Vector3f footPosition = target.AddZ(hull.m_Mins[2]);
		if(!NavigationGroundContentsSafe(footPosition, UseValidatedWalkEntries(),waterMiddleHeight)) NAV_WALK_REJECT();
		obTraceResult move;
		if(!EngineFuncs::TraceLine(move, target, target, &hull, bodyMask, GetGameID(), False) || move.m_StartSolid) NAV_WALK_REJECT();
		if(!EngineFuncs::TraceLine(move, position, target, &hull, bodyMask, GetGameID(), False)) NAV_WALK_REJECT();
		if(move.m_StartSolid) NAV_WALK_REJECT();
		if(UseValidatedWalkEntries() && move.m_Fraction < 1.f)
		{
			const Vector3f normal(move.m_Normal);
			const float normalLengthSq = normal.SquaredLength();
			const float inward = (target-position).Dot(normal);
			// A resting player is held .125u away from a wall by PMove's trace
			// epsilon. An otherwise valid node can lie .02u beyond that movement
			// boundary. Reproduce only this tiny wall-tangent slide, not a detour
			// around a real obstacle. Every corrected step is proved again below.
			if(Mathf::FAbs(normal.z) <= .001f && normalLengthSq >= .999f &&
				normalLengthSq <= 1.001f && inward < 0.f && inward >= -.125f)
			{
				if(tangentCorrections >= 8) NAV_WALK_REJECT();
				const Vector3f tangent = target-normal*inward;
				obTraceResult support, slide;
				if(!EngineFuncs::TraceLine(support, tangent.AddZ(18.f), tangent.AddZ(-18.5f),
					&feet, mask, GetGameID(), False) || support.m_StartSolid ||
					support.m_Fraction >= 1.f || !NavigationGroundSurfaceSafe(support.m_Normal[2],
						support.m_Surface,UseValidatedWalkEntries())) NAV_WALK_REJECT();
				const Vector3f supported = Vector3f(support.m_Endpos)+Vector3f(0,0,.125f);
				if(Mathf::FAbs(supported.z-tangent.z) > .25f ||
					!NavigationGroundContentsSafe(supported.AddZ(hull.m_Mins[2]),
						UseValidatedWalkEntries(),waterMiddleHeight)) NAV_WALK_REJECT();
				if(!EngineFuncs::TraceLine(slide, supported, supported, &hull, bodyMask,
					GetGameID(), False) || slide.m_StartSolid || slide.m_Fraction < 1.f) NAV_WALK_REJECT();
				if(!EngineFuncs::TraceLine(slide, position, supported, &hull, bodyMask,
					GetGameID(), False) || slide.m_StartSolid || slide.m_Fraction < 1.f) NAV_WALK_REJECT();
				target = supported;
				move = slide;
				++tangentCorrections;
				if(IsDebugEnabled(BOT_DEBUG_PLANNER))
					NAVSTUDIO_DIAGNOSTIC(va("TCE2WALK_TANGENT bot=%d step=%d/%d inward=%.5f position=(%.3f %.3f %.3f) corrected=(%.3f %.3f %.3f)",
						GetGameID(), i, steps, inward, position.x, position.y, position.z,
						target.x, target.y, target.z));
			}
		}
		if(move.m_Fraction < 1.f)
		{
			const Vector3f raised = position.AddZ(18.f);
			const Vector3f raisedTarget(target.x, target.y, raised.z);
			obTraceResult up;
			if(!EngineFuncs::TraceLine(up, position, raised, &hull,
				bodyMask, GetGameID(), False) || up.m_StartSolid || up.m_Fraction < 1.f) NAV_WALK_REJECT();
			if(!EngineFuncs::TraceLine(move, raised, raisedTarget,
				&hull, bodyMask, GetGameID(), False) || move.m_StartSolid || move.m_Fraction < 1.f) NAV_WALK_REJECT();
			if(!EngineFuncs::TraceLine(move, raisedTarget, target,
				&hull, bodyMask, GetGameID(), False) || move.m_StartSolid) NAV_WALK_REJECT();
			if(move.m_Fraction < 1.f)
			{
				if(move.m_Normal[2] < 0.7f || SquaredLength(Vector3f(move.m_Endpos), target) > 0.25f) NAV_WALK_REJECT();
				target = Vector3f(move.m_Endpos) + Vector3f(0,0,0.125f);
			}
		}
		position = target;
	}
	const Vector3f remainder = position-_destination;
	return Mathf::FAbs(remainder.z) <= 2.f &&
		remainder.x*remainder.x + remainder.y*remainder.y <= .25f*.25f;
#undef NAV_WALK_REJECT
}

bool Client::TurnTowardFacing(const Vector3f &_facing)
{
	return TurnTowardPosition(GetEyePosition() + _facing);
}

bool Client::TurnTowardPosition(const Vector3f &_pos)
{
	// return true if we're facing close enough
	Vector3f newFacing = (_pos - GetEyePosition());
	newFacing.Normalize();

	//{
	//	float heading = 0.f, pitch = 0.f, radius = 0.f;
	//	newFacing.ToSpherical(heading, pitch, radius);
	//	
	//	//Mathf::UnitCircleNormalize();
	//}


	if(newFacing == Vector3f::ZERO)
		return false;
	OBASSERT(m_FacingVector != Vector3f::ZERO, "Zero Facing Vector");

	// See how close we are to 
	float fDot = m_FacingVector.Dot(newFacing);

	// Determine the angle between the 2 different facings.
	float fAngle = Mathf::ACos(fDot);

	Ray3f rFacing(GetEyePosition(), GetFacingVector());
	DistVector3Ray3f dist(_pos, rFacing);
	float fDistance = dist.Get();

	// If it's very close, just snap it.
	bool bInTolerance = fDistance < m_AimTolerance ? true : false;

	// Calculate the frame time.
	const float fFrameTime = IGame::GetDeltaTimeSecs();

	// Calculate the Turn speed and clamp it to the max.
	const float fTurnSpeedRadians = Mathf::DegToRad(GetMaxTurnSpeed());
	m_CurrentTurnSpeed += (fFrameTime * ((m_AimStiffness * fAngle) - (m_AimDamping * m_CurrentTurnSpeed)));
	m_CurrentTurnSpeed = ClampT<float>(m_CurrentTurnSpeed, -fTurnSpeedRadians, fTurnSpeedRadians);

	OBASSERT(m_FacingVector != Vector3f::ZERO, "Zero Facing Vector");

	Quaternionf qQuat, qPartial;
	qQuat.Align(m_FacingVector, newFacing);

	if(fAngle > Mathf::ZERO_TOLERANCE)
	{
		qPartial.Slerp((m_CurrentTurnSpeed / fAngle) * fFrameTime, Quaternionf::IDENTITY, qQuat);
		m_FacingVector = qPartial.Rotate(m_FacingVector);
		m_FacingVector.Normalize();
	}
	else
	{
		qPartial = qQuat;		
		m_FacingVector = newFacing;
	}
	assert(m_FacingVector != Vector3f::ZERO);
	return bInTolerance;
}

bool Client::HasLineOfSightTo(const Vector3f &_pos, GameEntity _entity, int customTraceMask)
{
	Vector3f vStart = GetEyePosition();
	return HasLineOfSightTo(vStart, _pos, _entity, m_GameID, customTraceMask);
}

bool Client::HasLineOfSightTo(const Vector3f &_pos1, const Vector3f &_pos2, 
							  GameEntity _ent, int _ignoreent, int customTraceMask)
{
	Prof(HasLineOfSightTo);
	obTraceResult tr;
	EngineFuncs::TraceLine(tr, _pos1, _pos2, 
		NULL, customTraceMask ? customTraceMask : TR_MASK_SHOT | TR_MASK_SMOKEBOMB, _ignoreent, True);
	return (tr.m_Fraction == 1.0f) || ((_ent.IsValid()) && (tr.m_HitEntity == _ent));
}

bool Client::MoveTo(const Vector3f &_pos, float _tolerance, MoveMode _m)
{
	GetSteeringSystem()->SetTarget(_pos, _tolerance, _m);
	return ((_pos - GetPosition()).SquaredLength() <= (_tolerance * _tolerance));	
}

#ifdef FEATURE_NAVSTUDIO
void Client::EnableDebug(const int _flag, bool _enable)
{
	if(_enable)
	{
		m_DebugFlags.SetFlag(_flag);
	}
	else
	{
		m_DebugFlags.ClearFlag(_flag);
	}

	if(m_DebugFlags.CheckFlag(BOT_DEBUG_LOG))
	{
		if(IsDebugEnabled(BOT_DEBUG_LOG))
		{
			m_DebugLog.OpenForWrite(va("user/log_%s.rtf", GetName()), File::Text);

			if(m_DebugLog.IsOpen())
			{
				m_DebugLog.WriteString("Debug Log : ");
				m_DebugLog.WriteString(GetName());
				m_DebugLog.WriteNewLine();
			}
		} 
		else
		{
			m_DebugLog.Close();		
		}
	}	

	EngineFuncs::ConsoleMessage(va("debugging for %s: %s.", 
		GetName(), 
		IsDebugEnabled(_flag) ? "enabled" : "disabled"));
}
#endif


void Client::OutputDebug(MessageType _type, const char * _str)
{
#ifndef _DEBUG
	if(_type == kDebug)
		return;
#endif

	EngineFuncs::ConsoleMessage( va( "%s: %s", GetName(true), _str ) );

	if(IsDebugEnabled(BOT_DEBUG_LOG))
	{
		// dump to cout
		if(m_DebugLog.IsOpen())
		{
			m_DebugLog.WriteString(_str);
			m_DebugLog.WriteNewLine();
		}
	}
}

void Client::LoadProfile(ProfileType _type)
{
	String strProfileName;

	switch(_type) 
	{
	case PROFILE_CUSTOM:
		{
			const char *pName = GetName();
			if(pName)
			{
				strProfileName = NameManager::GetInstance()->GetProfileForName(pName);
			}
		}
		break;
	case PROFILE_CLASS:
		{
			strProfileName = NameManager::GetInstance()->GetProfileForClass(GetClass());
		}
		break;
	default:
		break;
	}

	// Load the profile.
	if(!strProfileName.empty())
	{
		if(!strProfileName.empty() && m_ScriptObject)
		{
			int threadId;
			gmVariable thisVar(m_ScriptObject);
			if(ScriptManager::GetInstance()->ExecuteFile(filePath("scripts/%s", strProfileName.c_str()), threadId, &thisVar) ||
				ScriptManager::GetInstance()->ExecuteFile(filePath("global_scripts/%s", strProfileName.c_str()), threadId, &thisVar))
			{
				DBG_MSG(BOT_DEBUG_SCRIPT, this, kNormal, va("Profile Loaded: %s", strProfileName.c_str()));
			} 
			else
			{
				DBG_MSG(BOT_DEBUG_SCRIPT, this, kError, va("Unable to load profile: %s", strProfileName.c_str()));
			}

			m_ProfileType = _type;
		}
	}
}

void Client::ClearProfile()
{
	// No class profile for this new class, so clear the profile
	ScriptManager::GetInstance()->RemoveFromGlobalTable(this);
	ScriptManager::GetInstance()->AddBotToGlobalTable(this);
	m_ProfileType = PROFILE_NONE;
}

void Client::ProcessEventImpl(const MessageHelper &_message, obuint32 _targetState)
{
	Prof(Client_ProcessEvent);

	gmMachine *pMachine = ScriptManager::GetInstance()->GetMachine();

	DisableGCInScope gcEn(pMachine);

	CallbackParameters cb(_message.GetMessageId(), pMachine);
	cb.SetTargetState(_targetState);
	ProcessEvent(_message, cb);

	if(IsDebugEnabled(BOT_DEBUG_EVENTS))
		cb.PrintDebug();

	// Events to Behavior Tree
	if(GetStateRoot() && cb.ShouldPropogateEvent())
		GetStateRoot()->CheckForCallbacks(_message, cb);
}

void Client::PropogateDeletedThreads(const int *_threadIds, int _numThreads)
{
	m_StateRoot->PropogateDeletedThreads(_threadIds, _numThreads);
}

bool Client::DistributeUnhandledCommand(const StringVector &_args)
{
	return m_StateRoot->StateCommand(_args);
}

void Client::ChangeTeam(int _team)
{
	// needs testing
	g_EngineFuncs->ChangeTeam(GetGameID(), _team, NULL);
}

void Client::ChangeClass(int _class)
{
	// needs testing
	g_EngineFuncs->ChangeClass(GetGameID(), _class, NULL);
}

Vector3f Client::ToLocalSpace(const Vector3f &_worldpos)
{
	Matrix3f mTransform(GetRightVector(), GetFacingVector(), GetUpVector(), true);
	mTransform.Inverse();
	return (_worldpos - GetPosition()) * mTransform;
}

Vector3f Client::ToWorldSpace(const Vector3f &_localpos)
{
	Matrix3f mTransform(GetRightVector(), GetFacingVector(), GetUpVector(), true);
	return GetPosition() + _localpos * mTransform;
}

void Client::GameCommand(const char* _msg)
{
	g_EngineFuncs->BotCommand(GetGameID(), _msg);
}

void Client::CheckTeamEvent()
{
	// Check our team.
	int iCurrentTeam = g_EngineFuncs->GetEntityTeam(GetGameEntity());
	if(iCurrentTeam != m_Team)
	{
		// Update our team.
		m_Team = iCurrentTeam;
		
		// Send a change team event.
		Event_ChangeTeam d = { iCurrentTeam };
		SendEvent(MessageHelper(MESSAGE_CHANGETEAM, &d, sizeof(d)));
	}
}

void Client::CheckClassEvent()
{
	int iCurrentClass = g_EngineFuncs->GetEntityClass(GetGameEntity());
	if(iCurrentClass != m_Class)
	{
		m_Class = iCurrentClass;
		
		// Send a change class event.
		Event_ChangeClass d = { iCurrentClass };
		SendEvent(MessageHelper(MESSAGE_CHANGECLASS, &d, sizeof(d)));
	}
}

bool Client::IsAllied(const GameEntity _ent) const
{
	return InterfaceFuncs::IsAllied(GetGameEntity(), _ent);
}

void Client::HoldButton(const BitFlag64 &_buttons, int _mstime)
{
	for(int i = 0; i < HoldButtons::NumButtons; ++i)
	{
		if(_buttons.CheckFlag(i))
		{
			m_HoldButtons.m_StopHoldTime[i] = 
				(_mstime > 0) ? IGame::GetTime() + _mstime : std::numeric_limits<int>::max();
		}
	}
}

void Client::ReleaseHeldButton(const BitFlag64 &_buttons)
{
	for(int i = 0; i < HoldButtons::NumButtons; ++i)
	{
		if(_buttons.CheckFlag(i))
		{
			m_HoldButtons.m_StopHoldTime[i] = 0;
		}
	}
}

void Client::ReleaseAllHeldButtons()
{
	for(int i = 0; i < HoldButtons::NumButtons; ++i)
	{
		m_HoldButtons.m_StopHoldTime[i] = 0;
	}
}

bool Client::CanGetPowerUp(obint32 _powerup) const
{
	return !GetPowerUpFlags().CheckFlag(_powerup);
}

//////////////////////////////////////////////////////////////////////////

#define INITSTATE(statename) \
	m_##statename = static_cast<statename*>(m_StateRoot->FindState(#statename)); OBASSERT(m_##statename, #statename " Not Found" );

void Client::InitBehaviorTree()
{
	m_StateRoot = new AiState::Root;
	m_StateRoot->FixRoot();
	SetupBehaviorTree();
	m_StateRoot->FixRoot();
	m_StateRoot->SetClient(this);

	using namespace AiState;
	INITSTATE(SensoryMemory);
	INITSTATE(SteeringSystem);
	INITSTATE(WeaponSystem);
	INITSTATE(TargetingSystem);
	INITSTATE(HighLevel);
	INITSTATE(LowLevel);
	INITSTATE(FollowPath);
	INITSTATE(Aimer);

	m_StateRoot->InitializeStates();
}

bool Client::AddScriptGoal(const String &_name)
{
	using namespace AiState;

	bool bSuccess = false;

	gmMachine *pMachine = ScriptManager::GetInstance()->GetMachine();
	const gmTableObject *pScriptGoals = pMachine->GetGlobals()->Get(pMachine, "ScriptGoals").GetTableObjectSafe();
	if(pScriptGoals)
	{
		gmVariable v = pScriptGoals->Get(pMachine, _name.c_str());
		if(v.m_type == gmScriptGoal::GetType())
		{
			gmScriptGoal::gmBindUserObject *pBnd = gmScriptGoal::GetUserBoundObject(pMachine, v);
			if(pBnd)
			{
				ScriptGoal *pNewScriptGoal = pBnd->m_object->Clone();

				if(pBnd->m_object->GetParentNameHash())
				{
					if(GetStateRoot()->AppendTo(pBnd->m_object->GetParentNameHash(), pNewScriptGoal))
					{
						pNewScriptGoal->FixRoot();
						pNewScriptGoal->SetClient(this);

						if(pNewScriptGoal->OnInit(pMachine))
						{
							bSuccess = true;
						}
						else
						{
							delete GetStateRoot()->RemoveState(pNewScriptGoal->GetName().c_str());
						}
					}
				}
				else if(pBnd->m_object->GetInsertBeforeHash())
				{
					if(GetStateRoot()->InsertBefore(pBnd->m_object->GetInsertBeforeHash(), pNewScriptGoal))
					{
						pNewScriptGoal->FixRoot();
						pNewScriptGoal->SetClient(this);

						if(pNewScriptGoal->OnInit(pMachine))
						{
							bSuccess = true;
						}
						else
						{
							delete GetStateRoot()->RemoveState(pNewScriptGoal->GetName().c_str());
						}
					}
				}
				else if(pBnd->m_object->GetInsertAfterHash())
				{
					if(GetStateRoot()->InsertBefore(pBnd->m_object->GetInsertAfterHash(), pNewScriptGoal))
					{
						pNewScriptGoal->FixRoot();
						pNewScriptGoal->SetClient(this);

						if(pNewScriptGoal->OnInit(pMachine))
						{
							bSuccess = true;
						}
						else
						{
							delete GetStateRoot()->RemoveState(pNewScriptGoal->GetName().c_str());
						}
					}
				}
			}
		}
	}
	return bSuccess;
}

void Client::InitScriptGoals()
{
	using namespace AiState;

	gmMachine *pMachine = ScriptManager::GetInstance()->GetMachine();
	const gmTableObject *pScriptGoals = pMachine->GetGlobals()->Get(pMachine, "ScriptGoals").GetTableObjectSafe();
	if(pScriptGoals)
	{
		gmTableIterator tIt;
		gmTableNode *pNode = pScriptGoals->GetFirst(tIt);
		while(pNode)
		{
			if(pNode->m_value.m_type == gmScriptGoal::GetType())
			{
				gmScriptGoal::gmBindUserObject *pBnd = gmScriptGoal::GetUserBoundObject(pMachine, pNode->m_value);
				if(pBnd)
				{
					if(pBnd->m_object->IsAutoAdd())
					{
						ScriptGoal *pNewScriptGoal = pBnd->m_object->Clone();
						if(pBnd->m_object->GetParentNameHash())
						{
							if(GetStateRoot()->AppendTo(pBnd->m_object->GetParentNameHash(), pNewScriptGoal))
							{
								pNewScriptGoal->FixRoot();
								pNewScriptGoal->SetClient(this);

								if(!pNewScriptGoal->OnInit(pMachine))
								{
									delete GetStateRoot()->RemoveState(pNewScriptGoal->GetName().c_str());
								}
							}
						}
						else if(pBnd->m_object->GetInsertBeforeHash())
						{
							if(GetStateRoot()->InsertBefore(pBnd->m_object->GetInsertBeforeHash(), pNewScriptGoal))
							{
								pNewScriptGoal->FixRoot();
								pNewScriptGoal->SetClient(this);

								if(!pNewScriptGoal->OnInit(pMachine))
								{
									delete GetStateRoot()->RemoveState(pNewScriptGoal->GetName().c_str());
								}
							}
						}
						else if(pBnd->m_object->GetInsertAfterHash())
						{
							if(GetStateRoot()->InsertBefore(pBnd->m_object->GetInsertAfterHash(), pNewScriptGoal))
							{
								pNewScriptGoal->FixRoot();
								pNewScriptGoal->SetClient(this);

								if(!pNewScriptGoal->OnInit(pMachine))
								{
									delete GetStateRoot()->RemoveState(pNewScriptGoal->GetName().c_str());
								}
							}
						}
					}
				}
			}
			pNode = pScriptGoals->GetNext(tIt);
		}
	}
}
