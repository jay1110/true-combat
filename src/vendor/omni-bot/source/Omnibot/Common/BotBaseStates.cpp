#include "PrecompCommon.h"
#include "BotBaseStates.h"
#include "ScriptManager.h"
#include "gmBotLibrary.h"
#include "PathPlannerWaypoint.h"

const obReal ROAM_GOAL_PRIORITY = 0.05f;

const float MIN_RENDER_TIME = 0.05f;

extern float g_fTopWaypointOffset;
extern float g_fBottomWaypointOffset;

namespace AiState
{
	//////////////////////////////////////////////////////////////////////////

	Roam::Roam() 
		: StateChild("Roam")
		, FollowPathUser("Roam")
	{
	}

	obReal Roam::GetPriority()
	{
		return ROAM_GOAL_PRIORITY; 
	}

	void Roam::Exit()
	{
		GetClient()->GetFollowPath()->Stop(true);
	}

	State::StateStatus Roam::Update(float fDt)
	{
		if(!GetClient()->GetFollowPath()->IsActive())
			GetClient()->GetFollowPath()->GotoRandomPt(this);
		return State_Busy;
	}

	//////////////////////////////////////////////////////////////////////////

	HighLevel::HighLevel() 
		: StatePrioritized("HighLevel")
	{
		AppendState(new Roam);
	}

	//////////////////////////////////////////////////////////////////////////

	FollowPathUser::FollowPathUser(const String &_user)
		: m_UserName(0)
		, m_CallingThread(GM_INVALID_THREAD)
		, m_DestinationIndex(0)
		, m_PathFailed(None)
		, m_PathSuccess(false)
	{
		m_UserName = Utils::MakeHash32(_user);
	}
	FollowPathUser::FollowPathUser(obuint32 _name)
		: m_UserName(_name)
		, m_CallingThread(GM_INVALID_THREAD)
		, m_PathFailed(None)
		, m_PathSuccess(false)
	{
	}

	void FollowPathUser::SetFollowUserName(obuint32 _name)
	{
		m_UserName = _name;
	}

	void FollowPathUser::SetFollowUserName(const String &_name)
	{
		SetFollowUserName(Utils::MakeHash32(_name));
	}

	//////////////////////////////////////////////////////////////////////////

	bool FollowPath::m_OldLadderStyle = false;

	FollowPath::FollowPath() 
		: StateChild("FollowPath")
		, m_PathStatus(PathFinished)
		, m_PtOnPath(Vector3f::ZERO)
		, m_LookAheadPt(Vector3f::ZERO)
		, m_TceCentreNavId(0)
		, m_TceCentreNextId(0)
		, m_TceCentreAttempts(0)
		, m_TceCentreStart(0)
		, m_TceCentreStillSince(0)
		, m_TceCentreLastProgress(0)
		, m_TceCentreBestDistance(0.f)
		, m_TceReentryAttempts(0)
		, m_TceReentryPlanning(false)
		, m_TceReentryPendingProgress(false)
		, m_TceReentryPosition(Vector3f::ZERO)
		, m_TceReentryBudgetPosition(Vector3f::ZERO)
		, m_TceReentryDestination(Vector3f::ZERO)
		, m_TceReentryBestRemaining(-1.f)
		, m_TceReentryCheckpointId(0)
		, m_TceAirWaitActive(false)
		, m_TceAirWaitStart(0)
		, m_TceOrdinaryLadderStart(0)
		, m_TceExitStartTime(0)
		, m_TceExitNavId(0)
		, m_TceDropStartTime(0)
		, m_TceDropNavId(0)
		, m_TceDropLaunchNavId(0)
		, m_TceDropSettleStart(0)
		, m_TceDropStableSince(0)
		, m_TceDropTrafficStart(0)
		, m_TceDropTrafficLast(0)
		, m_TceDropTrafficCredit(0)
		, m_TceDropTrafficNavId(0)
		, m_TceDropTrafficContact(false)
		, m_TceDropTrafficPosition(Vector3f::ZERO)
		, m_TceStandNavId(0)
		, m_TceStandPhase(0)
		, m_TceStandStart(0)
		, m_TceStandStable(0)
		, m_TceStandAttempt(0)
		, m_TceFreeNavId(0)
		, m_TceFreePhase(0)
		, m_TceFreeStart(0)
		, m_TceDoorPhase(0)
		, m_TceDoorCycles(0)
		, m_TceDoorWaitState(-1)
		, m_TceDoorPassageLogged(false)
		, m_TceDoorStart(0)
		, m_TceDoorPhaseStart(0)
		, m_TceDoorPulse(0)
		, m_TceDoorEgressStart(0)
		, m_TceDoorSweepWaitUntil(0)
		, m_TceDoorTraceEntity(-1)
		, m_TceDoorTraceUntil(0)
		, m_TceDoorTraceTime(0)
		, m_TceDoorBypassCount(0)
		, m_TceDoorSweepNavId(-1)
		, m_TceDoorSweepResult(-1)
		, m_JumpTime(0)
		, m_LastStuckTime(0)
		, m_PassThroughState(0)
		, m_RayDistance(-1.0f)
	{
	}

	void FollowPath::GetDebugString(StringStr &out)
	{
		if(m_Query.m_User && IsActive())
			out << Utils::HashToString(m_Query.m_User->GetFollowUserName());
		else
			out << "<none>";
	}

	void FollowPath::RenderDebug()
	{
		Utils::DrawLine(GetClient()->GetPosition(), m_PtOnPath, COLOR::BLUE, MIN_RENDER_TIME);
		Utils::DrawLine(GetClient()->GetPosition(), m_LookAheadPt, COLOR::MAGENTA, MIN_RENDER_TIME);
		m_CurrentPath.DebugRender(COLOR::RED, MIN_RENDER_TIME);
		Path::PathPoint pt;
		m_CurrentPath.GetCurrentPt(pt);
		Utils::DrawRadius(pt.m_Pt, pt.m_Radius, COLOR::GREEN, MIN_RENDER_TIME);
	}

	bool FollowPath::GetAimPosition(Vector3f &_aimpos)
	{
		_aimpos = m_LookAheadPt;
		if(m_TceDoorPhase == 2)
		{
			_aimpos = m_TceDoorEntry.m_TceDoor.aim;
			return true;
		}
		if(m_TceDoorPhase == 4 || (m_TceDoorPhase == 3 && m_CurrentPath.IsEndOfPath() &&
			SquaredLength2d(GetClient()->GetPosition(), m_TceDoorEntry.m_Pt) <= 1024.f))
		{
			// A return point behind a slightly overshooting bot reverses yaw
			// just when a timed door must be crossed. Use its proved passage
			// direction measured from the current eye, never that near point.
			Vector3f direction = (m_TceDoorExit.m_Pt - m_TceDoorEntry.m_Pt).Flatten();
			if(direction.Normalize() > 0.f) _aimpos = GetClient()->GetEyePosition() + direction * 128.f;
			return true;
		}
		Path::PathPoint pt, previous;
		if(GetClient()->UseValidatedWalkEntries() && m_CurrentPath.GetCurrentPt(pt) &&
			pt.m_TceWalkDrop)
		{
			Path::PathPoint landing;
			if(m_CurrentPath.GetNextPt(landing))
			{
				Vector3f direction = (landing.m_Pt - pt.m_Pt).Flatten();
				if(direction.Normalize() > 0.f)
					_aimpos = GetClient()->GetEyePosition() + direction * 128.f;
			}
			return true;
		}
		if(GetClient()->UseValidatedWalkEntries() && m_CurrentPath.GetCurrentPt(pt) &&
			m_CurrentPath.GetPreviousPt(previous) &&
			(((pt.m_NavFlags & F_NAV_CLIMB) && previous.m_TceAscentRelease) || previous.m_TceWalkDrop))
		{
			// A landing only a few units away is an unstable look target: the
			// next simulation frame changes eye height, and crossing it reverses
			// yaw back into the shaft. Use the checked exit direction, measured
			// from the current eye, until this action has actually completed.
			Vector3f direction = (pt.m_Pt - previous.m_Pt).Flatten();
			if(direction.Normalize() > 0.f)
				_aimpos = GetClient()->GetEyePosition() + direction * 128.f;
		}
		return true;
	}

	void FollowPath::OnTarget()
	{
	}

	void FollowPath::Enter()
	{
		m_LookAheadPt = GetClient()->GetEyePosition() + GetClient()->GetFacingVector() * 512.f;
		GetClient()->GetAimer()->AddAimRequest(Priority::Idle, this, GetNameHash());
	}
	void FollowPath::ResetTceFreeClimb()
	{
		m_TceFreeNavId=m_TceFreePhase=m_TceFreeStart=0;
	}
	void FollowPath::ResetTceCentreApproach()
	{
		m_TceCentreNavId = m_TceCentreNextId = m_TceCentreAttempts = 0;
		m_TceCentreStart = m_TceCentreStillSince = 0;
		m_TceCentreLastProgress = 0;
		m_TceCentreBestDistance = 0.f;
	}
	void FollowPath::ResetTceStandingJump()
	{
		m_TceStandNavId=m_TceStandPhase=m_TceStandStart=m_TceStandStable=m_TceStandAttempt=0;
	}

	void FollowPath::Exit()
	{
		Stop();

		GetClient()->GetSteeringSystem()->SetTarget(GetClient()->GetPosition());
		GetClient()->GetAimer()->ReleaseAimRequest(GetNameHash());
	}

	void FollowPath::Stop(bool _clearuser)
	{
		// _clearuser is true if HighLevel goal was aborted
		if(m_PassThroughState && _clearuser)
		{
			if(m_Query.m_User && m_Query.m_User->GetFollowUserName() == m_PassThroughState)
			{
				// don't interrupt active paththrough
				if(m_SavedQuery.m_User)
				{
					if(m_SavedQuery.m_User!=m_Query.m_User) 
						m_SavedQuery.m_User->OnPathFailed(FollowPathUser::Interrupted);
					m_SavedQuery.m_User = 0;
				}
				return;
			}
		}

		if(m_PathStatus == PathInProgress)
		{
			NotifyUserFailed(FollowPathUser::Interrupted);
		}

		m_PathStatus = PathFinished;
		m_CurrentPath.Clear();
		m_TceReentryAttempts = 0;
		m_TceReentryPlanning = m_TceReentryPendingProgress = false;
		m_TceReentryBestRemaining = -1.f;
		m_TceAirWaitActive = false;
		ResetTceDoor();
		ResetTceFreeClimb();
		ResetTceStandingJump();
		ResetTceCentreApproach();
		m_TceExitStartTime = m_TceExitNavId = 0;
		m_TceDropStartTime = m_TceDropNavId = 0;
		m_TceDropLaunchNavId = m_TceDropSettleStart = m_TceDropStableSince = 0;
		m_TceDropTrafficLast = m_TceDropTrafficCredit = 0;
		m_TceDropTrafficContact = false;
		// The contact lease deliberately survives Stop/Goto/Repath. Planning
		// again at the same blocked launch cannot buy another four seconds.

		if(_clearuser)
			ClearUser();
	}

	void FollowPath::ClearUser()
	{
		m_Query.m_User = 0;
	}

	void FollowPath::CancelPathThrough()
	{
		// always want to do this, if state active or not
		if(m_PassThroughState)
		{
			State *pPathThrough = GetClient()->GetLowLevel()->FindState(m_PassThroughState);
			if(pPathThrough)
				pPathThrough->EndPathThrough();
			m_PassThroughState = 0;
		}
	}

	void FollowPath::SaveQuery()
	{
		m_SavedQuery = m_Query;
	}

	void FollowPath::RestoreQuery()
	{
		m_Query = m_SavedQuery;
	}

	void FollowPath::ProcessEvent(const MessageHelper &_message, CallbackParameters &_cb)
	{
		switch(_message.GetMessageId())
		{
			HANDLER(MESSAGE_DYNAMIC_PATHS_CHANGED)
			{
				const Event_DynamicPathsChanged *m = _message.Get<Event_DynamicPathsChanged>();
				if(m != NULL && (m->m_TeamMask & 1<<GetClient()->GetTeam()))
				{
					DynamicPathUpdated(m);
				}
				break;
			}
		};
	}

	void FollowPath::DynamicPathUpdated(const Event_DynamicPathsChanged *_m)
	{
		const Path::PathPoint *p;
		for(int i = m_CurrentPath.GetCurrentPtIndex(); i < m_CurrentPath.GetNumPts(); ++i)
		{
			p = m_CurrentPath.GetPt(i);

			if(_m->m_NavId)
			{
				if(p->m_NavId==_m->m_NavId)
				{
					Repath();
					break;
				}
			}
			else if(_m->m_NavFlags)
			{
				if(p->m_NavFlags & _m->m_NavFlags)
				{
					Repath();
					break;
				}
			}
			else if(p->m_NavFlags&F_NAV_DYNAMIC)
			{
				Repath();
				break;
			}
		}
	}

	bool FollowPath::GotoRandomPt(FollowPathUser *_owner, MoveMode _movemode /*= Run*/)
	{
		PathPlannerBase *pPathPlanner = IGameManager::GetInstance()->GetNavSystem();
		Vector3f vDestination = pPathPlanner->GetRandomDestination(GetClient(),GetClient()->GetPosition(),GetClient()->GetTeamFlag());
		return Goto(_owner,vDestination);
	}

	bool FollowPath::Goto(FollowPathUser *_owner, MoveMode _movemode /*= Run*/, bool _skiplastpt /*= false*/)
	{
		m_IgnorePathNotFound = false;
		bool bFinalDest = true;
		if (!_owner) return false;

		DestinationVector destlist;
		if(_owner->GetNextDestination(destlist, bFinalDest, _skiplastpt))
		{
			return Goto(_owner, destlist, _movemode, _skiplastpt, bFinalDest);
		}

		if(_owner == m_Query.m_User)
		{
			m_PathStatus = PathNotFound;
			NotifyUserFailed(FollowPathUser::NoPath);
			m_Query.m_User = 0;
		}
		else
		{
			_owner->OnPathFailed(FollowPathUser::NoPath);
		}
		return false;
	}

	bool FollowPath::Goto(FollowPathUser *_owner, const Vector3f &_pos, float _radius /*= 32.f*/, MoveMode _movemode /*= Run*/, bool _skiplastpt /*= false*/)
	{
		m_IgnorePathNotFound = true;
		DestinationVector destlist;
		destlist.push_back(Destination(_pos,_radius));
		return Goto(_owner, destlist, _movemode, _skiplastpt);
	}

	bool FollowPath::Goto(FollowPathUser *_owner, const Vector3List &_goals, float _radius /*= 32.f*/, MoveMode _movemode /*= Run*/, bool _skiplastpt /*= false*/)
	{
		m_IgnorePathNotFound = true;
		DestinationVector destlist;
		for(obuint32 i = 0; i < _goals.size(); ++i)
		    destlist.push_back(Destination(_goals[i],_radius));
		return Goto(_owner, destlist, _movemode, _skiplastpt);
	}

	bool FollowPath::Goto(FollowPathUser *_owner, const MapGoalList &_goals, MoveMode _movemode /*= Run*/, bool _skiplastpt /*= false*/)
	{
		DestinationVector destlist;
		for(obuint32 i = 0; i < _goals.size(); ++i)
			destlist.push_back(Destination(_goals[i]->GetPosition(),_goals[i]->GetRadius()));
		return Goto(_owner, destlist, _movemode, _skiplastpt);
	}

	bool FollowPath::Goto(FollowPathUser *_owner, const DestinationVector &_goals, MoveMode _movemode /*= Run*/, bool _skiplastpt /*= false*/, bool _final /*= true*/)
	{
		if (m_PassThroughState && _owner != m_Query.m_User)
		{
			if(_owner->GetFollowUserName() == m_PassThroughState)
			{
				// paththrough called Goto,
				// save HighLevel goal's query
				OBASSERT(!m_SavedQuery.m_User, "m_SavedQuery overwritten");
				SaveQuery();
			}
			else if(m_Query.m_User && m_Query.m_User->GetFollowUserName() == m_PassThroughState)
			{
				if (m_SavedQuery.m_User && _owner != m_SavedQuery.m_User)
					m_SavedQuery.m_User->OnPathFailed(FollowPathUser::Interrupted);

				// remember current query and don't interrupt active paththrough
				m_SavedQuery.m_User = _owner;
				m_SavedQuery.m_Destination = _goals;
				m_SavedQuery.m_MoveMode = _movemode;
				m_SavedQuery.m_SkipLastPt = _skiplastpt;
				m_SavedQuery.m_Final = _final;
				return true;
			}
		}

		m_Query.m_User = _owner;
		m_Query.m_Destination = _goals;
		m_Query.m_MoveMode = _movemode;
		m_Query.m_SkipLastPt = _skiplastpt;
		m_Query.m_Final = _final;
		m_TceReentryAttempts = 0;
		m_TceReentryPlanning = m_TceReentryPendingProgress = false;
		m_TceReentryBestRemaining = -1.f;
		m_TceAirWaitActive = false;
		return Repath();
	}

	bool FollowPath::Repath()
	{
		if(!m_Query.m_User) return false;
		ResetTceDoor();
		ResetTceFreeClimb();
		ResetTceStandingJump();
		ResetTceCentreApproach();
		if(!m_TceReentryPlanning) m_Query.m_User->ResetPathUser();
		m_PathThroughPtIndex = -1;


		if(GetClient()->IsDebugEnabled(BOT_DEBUG_PLANNER))
		{
			String mgn;
			State *state = GetClient()->GetHighLevel()->GetActiveState();
			if(!state) mgn="not active";
			else {
				MapGoal *g = state->GetMapGoalPtr();
				if(g) mgn = g->GetName();
				else mgn = state->GetName();
			}
			const Vector3f &pos = GetClient()->GetPosition();
			EngineFuncs::ConsoleMessage(va("Path planner: time %.2f, position (%.0f,%.0f,%.0f), %s, %s %s", 
				IGame::GetTimeSecs(), pos.x, pos.y, pos.z, this->GetClient()->GetName(), mgn.c_str(),
				m_PassThroughState ? "- paththrough" : ""));
		}

		PathPlannerBase *pPathPlanner = IGameManager::GetInstance()->GetNavSystem();
		m_Query.m_User->m_DestinationIndex = pPathPlanner->PlanPathToNearestWithEntryPolicy(GetClient(),
			GetClient()->GetPosition(), m_Query.m_Destination, GetClient()->GetTeamFlag(), m_TceReentryPlanning);
		if(pPathPlanner->FoundGoal())
		{
			m_CurrentPath.Clear();
			pPathPlanner->GetPath(m_CurrentPath);
			if(!m_Query.m_SkipLastPt || m_CurrentPath.GetNumPts()==0)
			{
				Destination dest = m_Query.m_Destination[m_Query.m_User->m_DestinationIndex];
				// Planner points use waypoint display coordinates, whereas the
				// caller supplies a raw world-space destination (which may be an
				// item centre). Compare in planner space, then retain the original
				// centre/radius for the final live destination-volume walk proof.
				const float displayOffset = g_fBottomWaypointOffset + (g_fTopWaypointOffset-g_fBottomWaypointOffset) * .5f;
				const Vector3f displayDestination = dest.m_Position.AddZ(displayOffset);
				int num = m_CurrentPath.GetNumPts();
				if(num >= 2){
					Path::PathPoint pt1, pt2;
					m_CurrentPath.GetPt(num-1, pt2);
					if(Length(pt2.m_Pt, displayDestination) > pt2.m_Radius && !pt2.m_OnPathThrough){
						m_CurrentPath.GetPt(num-2, pt1);
						Vector3f vClosest;
						float t = Utils::ClosestPtOnLine(pt1.m_Pt, pt2.m_Pt, displayDestination, vClosest);
						if(t < 1.f && Length(vClosest, displayDestination) <= MaxT(dest.m_Radius, pt2.m_Radius)){
							//destination is near connection between last two waypoints
							m_CurrentPath.RemoveLastPt();
							m_CurrentPath.AddPt(vClosest, MinT(dest.m_Radius, pt2.m_Radius))
								.Flags(pt2.m_NavFlags)
								.NavId(pt2.m_NavId);
						}
					}
				}
				//append destination to path
				Path::PathPoint last;
				const NavFlags posture = m_CurrentPath.GetLastPt(last) ? last.m_NavFlags & (F_NAV_CROUCH | F_NAV_PRONE) : 0;
				m_CurrentPath.AddPt(dest.m_Position, dest.m_Radius).PlayerOrigin().Flags(posture)
					.TceDestinationVolume(GetClient()->UseValidatedWalkEntries());
			}
			if(!m_TceReentryPlanning) GetClient()->ResetStuckTime();
			m_PathStatus = PathInProgress;
		}
		else
		{
			if(m_TceReentryPlanning) return false;
			//path not found
			if(!m_IgnorePathNotFound) {
				State *state = GetClient()->GetHighLevel()->GetActiveState();
				if(state) {
					MapGoal *g = state->GetMapGoalPtr();
					if(g) {
						const Vector3f &pos = GetClient()->GetPosition();
						MapDebugPrint(va("Path not found from (%.0f,%.0f,%.0f) to %s", pos.x, pos.y, pos.z, g->GetName().c_str()));
					}
				}
			}

			m_PathStatus = PathNotFound;
			NotifyUserFailed(FollowPathUser::NoPath);
			if(!m_PassThroughState) m_Query.m_User = 0;
		}
		return m_PathStatus < PathFinished;
	}

	bool FollowPath::Goto(FollowPathUser *_owner, const Path &_path, MoveMode _movemode /*= Run*/)
	{
		if(!_owner)
			return false;
		m_TceReentryAttempts = 0;
		m_TceReentryPlanning = m_TceReentryPendingProgress = false;
		m_TceReentryBestRemaining = -1.f;
		m_TceAirWaitActive = false;
		ResetTceDoor();
		ResetTceFreeClimb();
		ResetTceStandingJump();
		ResetTceCentreApproach();
		//OBASSERT(_owner,"No User Defined!");

		m_Query.m_SkipLastPt = false;
		m_Query.m_Final = true;
		m_Query.m_User = _owner;
		m_Query.m_MoveMode = _movemode;
		m_Query.m_User->ResetPathUser();
		m_Query.m_Destination.resize(0);

		GetClient()->ResetStuckTime();
		m_CurrentPath = _path;
		m_PathStatus = PathInProgress;
		return true;
	}

	bool FollowPath::QueryPath(Path &pathOut, const DestinationVector &_goals, bool _skiplastpt, float _limitexpansion)
	{
		PathPlannerBase *pPathPlanner = IGameManager::GetInstance()->GetNavSystem();
		int iDestIndex = pPathPlanner->PlanPathToNearest(
			0, 
			GetClient()->GetPosition(), 
			_goals, 
			GetClient()->GetTeamFlag());

		if(pPathPlanner->FoundGoal())
		{
			pathOut.Clear();
			pPathPlanner->GetPath(pathOut);
			if(!_skiplastpt)
			{
				pathOut.AddPt(
					_goals[iDestIndex].m_Position, 
					_goals[iDestIndex].m_Radius).PlayerOrigin();
			}
		}
		else
		{
			return false;
		}
		return true;
	}

	bool FollowPath::IsMoving()
	{
		return !m_CurrentPath.IsEndOfPath();
	}

	void FollowPath::NotifyUserSuccess()
	{
		if(m_Query.m_User)
			m_Query.m_User->OnPathSucceeded();
	}

	void FollowPath::NotifyUserFailed(FollowPathUser::FailType _how)
	{
		if(m_Query.m_User)
			m_Query.m_User->OnPathFailed(_how);

		if(GetClient()->IsDebugEnabled(BOT_DEBUG_LOG_FAILED_PATHS))
		{
			const char *FailType = 0;
			switch(_how)
			{
			case FollowPathUser::Blocked:
				FailType = "Blocked";
				break;
			case FollowPathUser::NoPath:
				FailType = "Interrupted";
				break;
			case FollowPathUser::None:
			case FollowPathUser::Interrupted:
			default:
				break;
			}

			if(FailType)
			{
				File f;
				f.OpenForWrite(va("user/failedpaths.txt"), File::Text, true);
				if(f.IsOpen())
				{
					Vector3f Position = GetClient()->GetPosition();
					Vector3f Dest = Vector3f::ZERO;

					Path::PathPoint pt;
					if(m_CurrentPath.GetCurrentPt(pt))
						Dest = pt.m_Pt;

					f.WriteString("{"); 
					f.WriteNewLine();
					
					f.Printf("\tType = \"%s\",",FailType); f.WriteNewLine();
					f.Printf("\tP = Vector3(%f,%f,%f),",Position.x,Position.y,Position.z);
					f.WriteNewLine();
					
					if(_how == FollowPathUser::NoPath)
					{
						f.WriteString("\tDest = {"); f.WriteNewLine();
						for(obuint32 i = 0; i < m_Query.m_Destination.size(); ++i)
						{
							f.Printf("\t\tVector3(%f,%f,%f),",
								m_Query.m_Destination[i].m_Position.x,
								m_Query.m_Destination[i].m_Position.y,
								m_Query.m_Destination[i].m_Position.z);
							f.WriteNewLine();
						}
						f.WriteString("\t},"); f.WriteNewLine();
					}
					else
					{
						f.Printf("\tDest = Vector3(%f,%f,%f),",
							pt.m_Pt.x,pt.m_Pt.y,pt.m_Pt.z);
						f.WriteNewLine();
					}

					f.WriteString("},"); f.WriteNewLine();
					f.Close();
				}
			}
		}
	}

	obReal FollowPath::GetPriority() 
	{
		// always want to do this, if state active or not
		if(m_PassThroughState)
		{
			State *pPathThrough = GetClient()->GetLowLevel()->FindState(m_PassThroughState);
			if(!pPathThrough || (!pPathThrough->IsActive() && pPathThrough->GetLastPriority()<Mathf::EPSILON))
			{
				// paththrough state finished
				if(m_Query.m_User && m_Query.m_User->GetFollowUserName() == m_PassThroughState)
				{
					m_PassThroughState = 0;

					if(m_SavedQuery.m_User)
					{
						// find new path to the current HighLevel goal
						RestoreQuery();
						Repath();
					}
					else
					{
						Stop(false);
						m_Query.m_User = 0;
					}
				}
				else
				{
					m_PassThroughState = 0;

					if(m_CurrentPath.GetCurrentPtIndex() != m_PathThroughPtIndex + 1)
					{
						// repath if the previous waypoint has paththrough which has been skipped
						Path::PathPoint ptPrev;
						if(m_CurrentPath.GetPreviousPt(ptPrev) && ptPrev.m_OnPathThrough && ptPrev.m_OnPathThroughParam)
						{
							Repath();
						}
					}
				}
			}
		}
		return m_PathStatus < PathFinished ? (obReal)1.0 : (obReal)0.0; 
	}

	Vector3f RawWpPos(const Path::PathPoint &pp)
	{
		if(pp.m_IsPlayerOrigin) return pp.m_Pt;
		const float fWpHeight = g_fTopWaypointOffset - g_fBottomWaypointOffset;

		Vector3f v = pp.m_Pt;
		v.z -= fWpHeight*0.5f;
		v.z -= g_fBottomWaypointOffset;
		return v;
	}

	void FollowPath::ResetTceDoor()
	{
		m_TceDoorPhase = m_TceDoorCycles = m_TceDoorStart = m_TceDoorPhaseStart = m_TceDoorPulse = 0;
		m_TceDoorEgressStart = 0;
		m_TceDoorSweepWaitUntil = 0;
		m_TceDoorWaitState = -1;
		m_TceDoorPassageLogged = false;
		m_TceDoorBypassCount = 0;
		m_TceDoorSweepNavId = m_TceDoorSweepResult = -1;
		m_TceDoorSavedPath.Clear();
		GetClient()->ReleaseButton(BOT_BUTTON_USE);
		GetClient()->GetAimer()->ReleaseAimRequest(GetNameHash());
	}

	static int TceDoorStat(const GameEntity &entity, const char *name)
	{
		if(!entity.IsValid()) return -1;
		const obUserData value = InterfaceFuncs::GetEntityStat(entity, name);
		return value.IsInt() ? value.GetInt() : -1;
	}

	static int TceDoorPointSweep(Client *client, const Path::TceDoorRecipe &recipe, const Path::PathPoint &point)
	{
		AABB hull;
		if(!client->GetNavigationHull(hull, point.m_NavFlags)) return -1;
		const Vector3f feet = RawWpPos(point).AddZ(hull.m_Mins[2]);
		const float width = MaxT(MaxT(Mathf::FAbs(hull.m_Mins[0]),Mathf::FAbs(hull.m_Maxs[0])),
			MaxT(Mathf::FAbs(hull.m_Mins[1]),Mathf::FAbs(hull.m_Maxs[1])));
		char query[64];
		const int length = snprintf(query, sizeof(query), "tce_door_sweep:%d:%d:%.3f,%.3f,%.3f:%.0f:%.0f",
			recipe.entity, recipe.model, feet.x, feet.y, feet.z, width, hull.m_Maxs[2]-hull.m_Mins[2]);
		if(length < 0 || length >= (int)sizeof(query)) return -1;
		return TceDoorStat(client->GetGameEntity(), query);
	}

	static void TceDoorPathLog(const char *event, Client *client, const Path &path)
	{
		if(!client->IsDebugEnabled(BOT_DEBUG_FPINFO)) return;
		StringStr route;
		for(int i = path.GetCurrentPtIndex(); i < path.GetNumPts() && i < path.GetCurrentPtIndex()+24; ++i)
		{
			const Path::PathPoint *point = path.GetPt(i);
			route << (i == path.GetCurrentPtIndex() ? "" : ",") << point->m_NavId << ":" << point->m_TceDoor.role;
			if(point->m_IsPlayerOrigin) route << "p";
		}
		EngineFuncs::ConsoleMessage(va("TCE2DOOR PATH %s bot=%d cursor=%d points=%d route=%s",
			event, client->GetGameID(), path.GetCurrentPtIndex(), path.GetNumPts(), route.str().c_str()));
	}

	void FollowPath::TraceDoorMotion()
	{
		if(!GetClient()->IsDebugEnabled(BOT_DEBUG_FPINFO)) return;
		const int now = IGame::GetTime();
		if(m_TceDoorTraceEntity < 0 || now > m_TceDoorTraceUntil || now == m_TceDoorTraceTime) return;
		m_TceDoorTraceTime = now;
		const GameEntity door = g_EngineFuncs->EntityFromID(m_TceDoorTraceEntity);
		const Vector3f pos = GetClient()->GetPosition(), velocity = GetClient()->GetVelocity();
		const Vector3f move = GetClient()->GetMovementVector(), facing = GetClient()->GetFacingVector();
		const Vector3f target = GetClient()->GetSteeringSystem()->GetTarget();
		Path::PathPoint point; m_CurrentPath.GetCurrentPt(point);
		EngineFuncs::ConsoleMessage(va("TCE2DOOR MOTION bot=%d door=%d time=%d uid=%d phase=%d state=%d remaining=%d pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f) move=(%.3f %.3f %.3f) facing=(%.3f %.3f %.3f) steer=(%.3f %.3f %.3f) radius=%.3f",
			GetClient()->GetGameID(), m_TceDoorTraceEntity, now, point.m_NavId, m_TceDoorPhase,
			TceDoorStat(door, "tce_door_state"), TceDoorStat(door, "tce_door_open_remaining"),
			pos.x,pos.y,pos.z,velocity.x,velocity.y,velocity.z,move.x,move.y,move.z,
			facing.x,facing.y,facing.z,target.x,target.y,target.z,GetClient()->GetSteeringSystem()->GetTargetRadius()));
	}

	bool FollowPath::TceDoorRoute(int fromUid, int toUid, int phase)
	{
		PathPlannerBase *base = IGameManager::GetInstance()->GetNavSystem();
		if(!base || base->GetPlannerType() != NAVID_WP) return false;
		Path route;
		if(!static_cast<PathPlannerWaypoint*>(base)->BuildDoorRoute(GetClient(), fromUid, toUid, route)) return false;
		m_CurrentPath = route;
		m_TceDoorPhase = phase;
		m_TceDoorPhaseStart = IGame::GetTime();
		if(phase == 1) m_TceDoorPassageLogged = false;
		EngineFuncs::ConsoleMessage(va("TCE2DOOR %s bot=%d door=%d from=%d to=%d nodes=%d time=%d",
			phase == 1 ? (m_TceDoorEntry.m_TceDoor.mode == 3 ? "TO_LOCAL_USE" : "TO_SWITCH") : phase == 5 ? "BYPASS" : "RETURN", GetClient()->GetGameID(),
			m_TceDoorEntry.m_TceDoor.entity, fromUid, toUid, route.GetNumPts(), IGame::GetTime()));
		GetClient()->ResetStuckTime();
		return true;
	}

	// 0: ordinary path update, 1: this action owns steering, -1: bounded failure.
	int FollowPath::UpdateTceDoor(const Path::PathPoint &pt)
	{
		if(!GetClient()->UseValidatedWalkEntries()) return 0;
		if((pt.m_NavFlags & F_NAV_CROUCH) || (m_TceDoorPhase == 4 &&
			((m_TceDoorEntry.m_NavFlags | m_TceDoorExit.m_NavFlags) & F_NAV_CROUCH)))
			GetClient()->PressButton(BOT_BUTTON_CROUCH);
		const int now = IGame::GetTime();
		const Vector3f position = GetClient()->GetPosition();
		if(!m_TceDoorPhase)
		{
			if(pt.m_TceDoor.marked && (pt.m_TceDoor.role == 1 || pt.m_TceDoor.role == 2) &&
                !pt.m_TceDoor.Valid()) return -1;
            if(pt.m_TceDoor.Valid() && pt.m_TceDoor.mode == 3 &&
                (pt.m_TceDoor.role == 1 || pt.m_TceDoor.role == 2))
            {
                const bool atUse = pt.m_TceDoor.role == 1;
                Path::PathPoint entry = pt, exit;
                if(atUse && !m_CurrentPath.GetNextPt(entry)) return -1;
                const int exitIndex = m_CurrentPath.GetCurrentPtIndex()+(atUse ? 2 : 1);
                if(exitIndex >= m_CurrentPath.GetNumPts()) return -1;
                const Path::PathPoint *after = m_CurrentPath.GetPt(exitIndex);
                if(!after || !entry.m_TceDoor.Valid() || entry.m_TceDoor.role != 2 ||
                    !pt.m_TceDoor.SameDoor(entry.m_TceDoor) || entry.m_NavId != entry.m_TceDoor.entry ||
                    after->m_NavId != entry.m_TceDoor.target ||
                    (atUse && (pt.m_NavId != entry.m_TceDoor.useNav || pt.m_TceDoor.target != entry.m_NavId))) return -1;
                exit = *after;
                PathPlannerBase *base = IGameManager::GetInstance()->GetNavSystem();
                if(!base || base->GetPlannerType() != NAVID_WP) return -1;
                Waypoint *use = static_cast<PathPlannerWaypoint*>(base)->GetWaypointByGUID(entry.m_TceDoor.useNav);
                const Path::TceDoorRecipe useRecipe = PathPlannerWaypoint::ReadDoorRecipe(use);
                if(!use || !useRecipe.Valid() || useRecipe.role != 1 ||
                    !entry.m_TceDoor.SameDoor(useRecipe) || useRecipe.target != entry.m_NavId) return -1;
                m_TceDoorUse = Path::PathPoint().Position(use->GetPosition()).PlayerOrigin()
                    .NavId(use->GetUID()).Flags(use->GetNavigationFlags()).Radius(2.f).TceDoor(useRecipe);
                if(TceDoorPointSweep(GetClient(), entry.m_TceDoor, m_TceDoorUse) != 1) return -1;
                m_TceDoorEntry = entry; m_TceDoorExit = exit;
                m_TceDoorSavedPath = m_CurrentPath;
                if(atUse && !m_TceDoorSavedPath.NextPt()) return -1;
                m_TceDoorStart = now; m_TceDoorCycles = 0;
                TceDoorPathLog("SAVE_LOCAL", GetClient(), m_TceDoorSavedPath);
                // Replanning may begin at ENTRY. Retreat only if the actual
                // full-body ordinary walk to this checked USE is clear now.
                if(!TceDoorRoute(use->GetUID(), use->GetUID(), 1)) return -1;
                GetClient()->ReleaseButton(BOT_BUTTON_USE);
                GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorUse),0.f,m_Query.m_MoveMode,false);
                return 1;
            }
			if(!pt.m_TceDoor.Valid() || pt.m_TceDoor.role != 2) return 0;
			Path::PathPoint exit;
			if(!m_CurrentPath.GetNextPt(exit) || exit.m_NavId != pt.m_TceDoor.target ||
				pt.m_NavId != pt.m_TceDoor.entry) return -1;
			if(!m_TceDoorStart) m_TceDoorStart = now;
			// Always reach the checked entry centre before opening a door or
			// diverting to its button. An arrival radius is not a door shortcut.
			if(SquaredLength2d(position, RawWpPos(pt)) > 4.f || Mathf::FAbs(position.z - RawWpPos(pt).z) > 2.f ||
				!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND))
			{
				if(now - m_TceDoorStart > 10000) return -1;
				if(!GetClient()->CanWalkTo(RawWpPos(pt), pt.m_NavFlags))
				{
					const GameEntity door = g_EngineFuncs->EntityFromID(pt.m_TceDoor.entity);
					const int state = TceDoorStat(door, "tce_door_state");
					if(state < 1 || state > 3 || TceDoorStat(door, "tce_nav_model") != pt.m_TceDoor.model) return -1;
					// A rotating leaf can temporarily cross its otherwise proved
					// closed-side approach. Wait for real collision to clear it.
					if(m_TceDoorWaitState != state)
					{
						m_TceDoorWaitState = state;
						EngineFuncs::ConsoleMessage(va("TCE2DOOR WAIT_APPROACH bot=%d door=%d state=%d entry=%d time=%d",
							GetClient()->GetGameID(), pt.m_TceDoor.entity, state, pt.m_NavId, now));
					}
					GetClient()->ResetStuckTime();
					GetClient()->GetSteeringSystem()->SetTarget(position);
					return 1;
				}
				GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(pt), 0.f, m_Query.m_MoveMode, false);
				if(pt.m_NavFlags & F_NAV_CROUCH) GetClient()->PressButton(BOT_BUTTON_CROUCH);
				return 1;
			}
			m_TceDoorEntry = pt;
			m_TceDoorExit = exit;
			m_TceDoorSavedPath = m_CurrentPath;
			TceDoorPathLog("SAVE", GetClient(), m_TceDoorSavedPath);
			m_TceDoorPhaseStart = now;
			m_TceDoorPhase = 4;
			m_TceDoorCycles = 0;
		}
		const Path::TceDoorRecipe &recipe = m_TceDoorEntry.m_TceDoor;
		// The precise EXIT and bypass branches also require the current mover
		// identity. A map restart must not reuse cached metadata for an entity
		// slot which now holds another model or an invalid mover.
		const GameEntity observedDoor = g_EngineFuncs->EntityFromID(recipe.entity);
		if(TceDoorStat(observedDoor, "tce_nav_model") != recipe.model ||
			TceDoorStat(observedDoor, "tce_door_state") < 0) return -1;
		if(m_TceDoorPhase == 5)
		{
			GetClient()->ReleaseButton(BOT_BUTTON_USE);
			if(now-m_TceDoorStart > 45000) return -1;
			if(!m_CurrentPath.IsEndOfPath()) return 0;
			const Vector3f exit = RawWpPos(m_TceDoorExit);
			if(SquaredLength2d(position,exit) > 4.f || Mathf::FAbs(position.z-exit.z)>2.f ||
				!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND))
			{
				GetClient()->GetSteeringSystem()->SetTarget(exit,0.f,m_Query.m_MoveMode,false);
				return 1;
			}
			m_CurrentPath=m_TceDoorSavedPath;
			if(!m_CurrentPath.NextPt()) return -1;
			m_TceDoorPhase=4;
			TceDoorPathLog("BYPASS_RESUME",GetClient(),m_CurrentPath);
			return 1;
		}
		if(m_TceDoorPhase == 4 && pt.m_NavId == recipe.target &&
			SquaredLength2d(position, RawWpPos(m_TceDoorExit)) <= 4.f &&
			Mathf::FAbs(position.z-RawWpPos(m_TceDoorExit).z) <= 2.f &&
			GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND))
		{
			Path::PathPoint next;
			const bool nextPresent=m_CurrentPath.GetNextPt(next);
			if(nextPresent)
			{
				if(m_TceDoorSweepNavId!=next.m_NavId)
				{
					m_TceDoorSweepNavId=next.m_NavId;
					m_TceDoorSweepResult=TceDoorPointSweep(GetClient(),recipe,next);
				}
				if(m_TceDoorSweepResult<0) return -1;
				const GameEntity liveDoor=g_EngineFuncs->EntityFromID(recipe.entity);
				const int state=TceDoorStat(liveDoor,"tce_door_state");
				const int remaining=TceDoorStat(liveDoor,"tce_door_open_remaining");
				// Endpoint clearance alone cannot prove the complete ordinary
				// EXIT->NEXT segment stays outside the closing leaf sweep. Remain
				// at the native safe EXIT until a timed mover actually closes.
				// An untimed, genuinely open door still uses the live body sweep.
				if(state!=0 && !(state==2&&remaining<0))
				{
					if(!m_TceDoorSweepWaitUntil)
					{
						if(remaining > 45000 || remaining >= 45000-(now-m_TceDoorStart)) return -1;
						m_TceDoorSweepWaitUntil=MinT(m_TceDoorStart+45000,
							now+MaxT(remaining,0)+8000);
						EngineFuncs::ConsoleMessage(va("TCE2DOOR WAIT_SWEEP bot=%d door=%d exit=%d next=%d state=%d remaining=%d time=%d",
							GetClient()->GetGameID(),recipe.entity,recipe.target,next.m_NavId,state,remaining,now));
					}
					GetClient()->ResetStuckTime();GetClient()->GetSteeringSystem()->SetTarget(position);
					GetClient()->ReleaseButton(BOT_BUTTON_USE);
					return now>m_TceDoorSweepWaitUntil || now-m_TceDoorStart>45000 ? -1:1;
				}
				m_TceDoorSweepWaitUntil=0;
			}
			const NavFlags actions = F_NAV_CLIMB | F_NAV_JUMP | F_NAV_JUMPLOW |
				F_NAV_JUMPGAP | F_NAV_TELEPORT | F_NAV_ELEVATOR | F_NAV_SCRIPT;
			if(nextPresent && !((pt.m_NavFlags | next.m_NavFlags) & actions) &&
				!GetClient()->CanWalkTo(RawWpPos(next), pt.m_NavFlags | next.m_NavFlags))
			{
				if(!m_TceDoorEgressStart)
				{
					m_TceDoorEgressStart = now;
					EngineFuncs::ConsoleMessage(va("TCE2DOOR WAIT_EGRESS bot=%d door=%d exit=%d next=%d time=%d",
						GetClient()->GetGameID(), recipe.entity, recipe.target, next.m_NavId, now));
				}
				GetClient()->ResetStuckTime();
				GetClient()->GetSteeringSystem()->SetTarget(position);
				return now-m_TceDoorEgressStart > 8000 || now-m_TceDoorStart > 45000 ? -1 : 1;
			}
			// Once physically outside the leaf, opening is no longer required.
			// A naturally closed door must not fail an already crossed passage.
			EngineFuncs::ConsoleMessage(va("TCE2DOOR %s bot=%d door=%d exit=%d pos=(%.3f %.3f %.3f) time=%d",
				m_TceDoorBypassCount ? "BYPASS_EGRESS" : "EGRESS", GetClient()->GetGameID(),
				recipe.entity, recipe.target, position.x, position.y, position.z, now));
			m_TceDoorTraceEntity = recipe.entity;
			m_TceDoorTraceUntil = now+1500;
			m_TceDoorTraceTime = 0;
			const bool haveNext = nextPresent;
			if(haveNext)
			{
				// Advance this completed action before yielding. The caller's
				// local pt is still EXIT; using it for one more ordinary update
				// can steer back toward a reciprocal door action.
				m_CurrentPath.NextPt();
				GetClient()->ResetStuckTime();
				const Vector3f target = RawWpPos(next);
				GetClient()->GetSteeringSystem()->SetTarget(target, MinT(next.m_Radius, 1.f), m_Query.m_MoveMode, false);
				m_LookAheadPt = target;
				m_LookAheadPt.z = GetClient()->GetEyePosition().z;
			}
			TceDoorPathLog("EGRESS", GetClient(), m_CurrentPath);
			ResetTceDoor();
			return haveNext ? 1 : 0;
		}
		const GameEntity door = g_EngineFuncs->EntityFromID(recipe.entity);
		const GameEntity use = g_EngineFuncs->EntityFromID(recipe.useEntity);
		const int kind = TceDoorStat(use, "tce_nav_use_kind");
		const int team = GetClient()->GetTeam();
		if(now - m_TceDoorStart > 45000 || team < 1 || team > 2 || !(recipe.team & team) ||
			TceDoorStat(door, "tce_nav_model") != recipe.model ||
			TceDoorStat(use, "tce_nav_model") != recipe.useModel ||
			TceDoorStat(use, va("tce_use_door:%d:%d", recipe.entity, recipe.model)) != 1 ||
			TceDoorStat(door, "tce_door_locked") != 0 ||
			!(TceDoorStat(door, "tce_door_team") & team) ||
			(recipe.mode == 2 ? !(kind == 3 || kind == 4) : !(kind == 1 || kind == 2))) return -1;
		const int state = TceDoorStat(door, "tce_door_state");
		if(state < 0) return -1;
		if(state != 1 && state != 3) m_TceDoorWaitState = -1;
		if((m_TceDoorPhase == 2 || m_TceDoorPhase == 4) && (state == 1 || state == 3) &&
			m_TceDoorWaitState != state)
		{
			m_TceDoorWaitState = state;
			EngineFuncs::ConsoleMessage(va("TCE2DOOR WAIT bot=%d door=%d state=%s phase=%d time=%d",
				GetClient()->GetGameID(), recipe.entity, state == 1 ? "opening" : "closing", m_TceDoorPhase, now));
		}
		GetClient()->ReleaseButton(BOT_BUTTON_USE);

        if(recipe.mode == 3 && m_TceDoorPhase == 3 && state != 2)
        {
            // ENTRY may lie inside the sweep: never continue an opening or
            // closing return leg. Only a currently clear walk can retreat.
            if(++m_TceDoorCycles > 2 || !TceDoorRoute(recipe.useNav,recipe.useNav,1)) return -1;
            GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorUse),0.f,m_Query.m_MoveMode,false);
            return 1;
        }
		if(m_TceDoorPhase == 1 || m_TceDoorPhase == 3)
		{
			if(!m_CurrentPath.IsEndOfPath()) return 0;
			const Vector3f destination = RawWpPos(pt);
			if(SquaredLength2d(position, destination) > 4.f || Mathf::FAbs(position.z-destination.z) > 2.f ||
				!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND))
			{
				GetClient()->GetSteeringSystem()->SetTarget(destination, 0.f, m_Query.m_MoveMode, false);
				if(pt.m_NavFlags & F_NAV_CROUCH) GetClient()->PressButton(BOT_BUTTON_CROUCH);
				return 1;
			}
			if(m_TceDoorPhase == 3)
			{
				m_CurrentPath = m_TceDoorSavedPath;
				TceDoorPathLog("RESUME", GetClient(), m_CurrentPath);
				m_TceDoorPhase = 4;
				GetClient()->ResetStuckTime();
				return 1;
			}
			m_TceDoorPhase = 2;
			m_TceDoorPhaseStart = now;
			m_TceDoorPulse = 0;
		}
		if(m_TceDoorPhase == 2)
		{
			GetClient()->ResetStuckTime();
            if(recipe.mode == 3)
            {
                if(SquaredLength2d(position, RawWpPos(m_TceDoorUse)) > 4.f ||
                    Mathf::FAbs(position.z-RawWpPos(m_TceDoorUse).z) > 2.f ||
                    !GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND))
                {
                    if(now-m_TceDoorPhaseStart > 8000 ||
                        !GetClient()->CanWalkTo(RawWpPos(m_TceDoorUse),m_TceDoorUse.m_NavFlags)) return -1;
                    GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorUse),0.f,m_Query.m_MoveMode,false);
                    return 1;
                }
                Path::PathPoint actual = m_TceDoorUse;
                actual.Position(position).PlayerOrigin();
                if(TceDoorPointSweep(GetClient(),recipe,actual) != 1) return -1;
            }
			if(recipe.mode == 1 && (SquaredLength2d(position, RawWpPos(m_TceDoorEntry)) > 4.f ||
				Mathf::FAbs(position.z-RawWpPos(m_TceDoorEntry).z) > 2.f))
			{
				if(now-m_TceDoorPhaseStart > 8000 ||
					!GetClient()->CanWalkTo(RawWpPos(m_TceDoorEntry), m_TceDoorEntry.m_NavFlags)) return -1;
				GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorEntry), 0.f, m_Query.m_MoveMode, false);
				GetClient()->GetAimer()->AddAimRequest(Priority::High, this, GetNameHash());
				return 1;
			}
			if(recipe.mode == 1)
			{
				const int forced = TceDoorStat(door, "tce_door_force");
				if(forced < 0) return -1;
				if(forced)
				{
					// A FORCE rotor cannot choose an opening away from its user.
					// Verify the actual actor's entire wait body before any USE.
					if(m_TceDoorSweepNavId != recipe.entry)
					{
						m_TceDoorSweepNavId = recipe.entry;
						m_TceDoorSweepResult = TceDoorPointSweep(GetClient(), recipe, m_TceDoorEntry);
					}
					if(m_TceDoorSweepResult != 1) return -1;
				}
			}
			GetClient()->GetSteeringSystem()->SetTarget(position);
			GetClient()->GetAimer()->AddAimRequest(Priority::High, this, GetNameHash());
			if(pt.m_NavFlags & F_NAV_CROUCH) GetClient()->PressButton(BOT_BUTTON_CROUCH);
            if(state == 2 || (recipe.mode==2 && state==1))
            {
                if(recipe.mode == 3)
                {
                    const bool crouch=((m_TceDoorUse.m_NavFlags|m_TceDoorEntry.m_NavFlags|m_TceDoorExit.m_NavFlags)&F_NAV_CROUCH)!=0;
                    const int speed=TceDoorStat(GetClient()->GetGameEntity(),va("tce_door_speed:%d:%d",crouch?1:0,m_Query.m_MoveMode==Walk?1:0));
                    const int remaining=TceDoorStat(door,"tce_door_open_remaining");
                    if(speed<=0 || remaining < -1) return -1;
                    const int needed=(int)ceilf(1000.f*(Length(position,RawWpPos(m_TceDoorEntry))+
                        Length(RawWpPos(m_TceDoorEntry),RawWpPos(m_TceDoorExit)))/speed)+500;
                    if(remaining>=0 && remaining<needed)
                    {
                        // Stay at the full-sweep-safe USE until the short
                        // opening closes; a later pulse may start a full cycle.
                        return now-m_TceDoorPhaseStart>8000 ? -1 : 1;
                    }
                }
                if(recipe.mode == 2 || recipe.mode == 3)
				{
					if(!TceDoorRoute(recipe.useNav, recipe.entry, 3)) return -1;
				}
				else m_TceDoorPhase = 4;
				return 1;
			}
			if(now - m_TceDoorPhaseStart > 8000) return -1;
			const int useIdentity = state == 0 ?
				TceDoorStat(GetClient()->GetGameEntity(), "tce_use_identity") : -1;
			if(state == 0 && useIdentity != recipe.useEntity && (recipe.mode == 1 || recipe.mode == 3))
			{
				// Direct USE has a strict native 38.4-unit eye-ray range. Being
				// inside the 2-unit arrival radius can still put that exact ray
				// out of range (Northport 378: 38.701 instead of <=38.4). Move
				// precisely to the proved USE, never lengthen/replace the ray.
				const Path::PathPoint &usePoint = recipe.mode == 3 ? m_TceDoorUse : m_TceDoorEntry;
				const Vector3f target = RawWpPos(usePoint);
				if(!GetClient()->CanWalkTo(target, usePoint.m_NavFlags)) return -1;
				const float distance = Mathf::Sqrt(SquaredLength2d(position, target));
				// No positional dead zone while the strict eye identity is still
				// false: Northport 379 remained 0.118u off centre and its correct
				// door hit was still 38.441856u away. Native identity, not a small
				// geometric radius, decides when these bounded inputs may stop.
				GetClient()->GetSteeringSystem()->SetTarget(target, 0.f, m_Query.m_MoveMode, false,
					ClampT(distance / 32.f, .01f, .125f));
				// The original eight-second phase limit also bounds stationary
				// aiming or a persistently invalid identity at the true centre.
				return 1;
			}
			// Press for exactly one bot update, separated by release frames.
			// The native read-only eye ray must identify this exact checked user.
			if(state == 0 && now - m_TceDoorPulse >= 500 &&
				useIdentity == recipe.useEntity)
			{
				GetClient()->PressButton(BOT_BUTTON_USE);
				m_TceDoorPulse = now;
				EngineFuncs::ConsoleMessage(va("TCE2DOOR USE bot=%d door=%d use=%d entry=%d cycle=%d",
					GetClient()->GetGameID(), recipe.entity, recipe.useEntity, recipe.entry, m_TceDoorCycles));
			}
			return 1;
		}
		if(m_TceDoorPhase == 4)
		{
			if(pt.m_NavId != recipe.entry && pt.m_NavId != recipe.target)
			{
				ResetTceDoor();
				return 0;
			}
            if(recipe.mode == 3 && pt.m_NavId == recipe.entry && state != 2)
            {
                if(++m_TceDoorCycles > 2 || !TceDoorRoute(recipe.useNav,recipe.useNav,1)) return -1;
                GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorUse),0.f,m_Query.m_MoveMode,false);
                return 1;
            }
			const bool closedAfterReturn = state==0 && recipe.mode==2 && m_TceDoorCycles>0;
			if(pt.m_NavId==recipe.entry && (state==2 || closedAfterReturn) && !m_TceDoorBypassCount)
			{
				const int remaining=closedAfterReturn ? 0 : TceDoorStat(door,"tce_door_open_remaining");
				const bool crouch=((m_TceDoorEntry.m_NavFlags|m_TceDoorExit.m_NavFlags)&F_NAV_CROUCH)!=0;
				const int speed=TceDoorStat(GetClient()->GetGameEntity(),va("tce_door_speed:%d:%d",crouch?1:0,m_Query.m_MoveMode==Walk?1:0));
				if(speed<=0) return -1;
				const int needed=(int)ceilf(1000.f*Length(position,RawWpPos(m_TceDoorExit))/speed)+250;
                if(remaining>=0 && remaining<needed)
                {
                    if(recipe.mode == 3)
                    {
                        if(++m_TceDoorCycles > 2 || !TceDoorRoute(recipe.useNav,recipe.useNav,1)) return -1;
                        GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorUse),0.f,m_Query.m_MoveMode,false);
                        return 1;
                    }
                    ++m_TceDoorBypassCount;
					EngineFuncs::ConsoleMessage(va("TCE2DOOR WINDOW_BYPASS bot=%d door=%d remaining=%d needed=%d speed=%d time=%d",
						GetClient()->GetGameID(),recipe.entity,remaining,needed,speed,now));
					if(!TceDoorRoute(recipe.entry,recipe.target,5)) return -1;
					return 1;
				}
			}
			const bool exitWalk = GetClient()->CanWalkTo(RawWpPos(m_TceDoorExit),
				m_TceDoorEntry.m_NavFlags | m_TceDoorExit.m_NavFlags);
			if((state == 2 || pt.m_NavId == recipe.target) && exitWalk)
			{
				if(!m_TceDoorPassageLogged && !m_TceDoorBypassCount)
				{
					m_TceDoorPassageLogged = true;
					EngineFuncs::ConsoleMessage(va("TCE2DOOR PASSAGE bot=%d door=%d entry=%d exit=%d pos=(%.3f %.3f %.3f) time=%d",
						GetClient()->GetGameID(), recipe.entity, recipe.entry, recipe.target, position.x, position.y, position.z, now));
				}
				if(pt.m_NavId == recipe.target)
				{
					// Keep this node until its real outside centre is reached;
					// radius-only arrival can still be beside the moving leaf.
					GetClient()->GetSteeringSystem()->SetTarget(RawWpPos(m_TceDoorExit), 0.f, m_Query.m_MoveMode, false);
					m_LookAheadPt = RawWpPos(m_TceDoorExit);
					m_LookAheadPt.z = GetClient()->GetEyePosition().z;
					GetClient()->GetAimer()->AddAimRequest(Priority::High, this, GetNameHash());
					return 1;
				}
				return 0;
			}
			// Do not move into a closing leaf. If a timer expired while returning
			// from a remote switch, retry along the already checked closed route.
			GetClient()->ResetStuckTime();
			GetClient()->GetSteeringSystem()->SetTarget(position);
			if(state == 1 || state == 3) return now-m_TceDoorStart > 30000 ? -1 : 1;
			if(state == 2) return now-m_TceDoorStart > 10000 ? -1 : 1; // real traffic/obstacle
			if(pt.m_NavId != recipe.entry)
			{
				// A closing leaf can push a bot back to its original side after
				// the ENTRY point was consumed. Retry only when the actual closed
				// world still permits walking back to that exact checked entry.
				if(!GetClient()->CanWalkTo(RawWpPos(m_TceDoorEntry), m_TceDoorEntry.m_NavFlags)) return -1;
				m_CurrentPath = m_TceDoorSavedPath;
			}
			if(++m_TceDoorCycles > 2) return -1;
			m_TceDoorPassageLogged = false;
            if(recipe.mode == 3)
            {
                if(!TceDoorRoute(recipe.useNav,recipe.useNav,1)) return -1;
            }
            else if(recipe.mode == 2)
            {
                if(!TceDoorRoute(recipe.entry, recipe.useNav, 1)) return -1;
			}
			else { m_TceDoorPhase = 2; m_TceDoorPhaseStart = now; m_TceDoorPulse = 0; }
			return 1;
		}
		return -1;
	}

	bool FollowPath::TryTceReentry(const Path::PathPoint &blocked)
	{
		// Combat can push the actor off an otherwise valid path segment. Use
		// the real planner's bounded native entry search, not a blind return to
		// the old centre. Two attempts are available until proved, substantial
		// progress along ordinary waypoints renews that bounded budget.
		if(!GetClient()->UseValidatedWalkEntries() || !m_Query.m_User || m_PassThroughState ||
			m_TceDoorPhase || m_TceDropNavId || m_TceStandPhase || m_TceFreePhase ||
			m_TceReentryAttempts >= 2 || GetClient()->GetStuckTime() >= 2000 ||
			!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) || GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER)) return false;
		const int destinationIndex = m_Query.m_User->m_DestinationIndex;
		if(destinationIndex < 0 || destinationIndex >= (int)m_Query.m_Destination.size()) return false;
		const Path oldPath = m_CurrentPath;
		const PathStatus oldStatus = m_PathStatus;
		const DestinationVector destinations = m_Query.m_Destination;
		m_Query.m_Destination.clear();
		m_Query.m_Destination.push_back(destinations[destinationIndex]);
		++m_TceReentryAttempts;
		m_TceReentryPlanning = true;
		const bool planned = Repath();
		m_TceReentryPlanning = false;
		m_Query.m_Destination = destinations;
		m_Query.m_User->m_DestinationIndex = destinationIndex;
		Path::PathPoint entry;
		const bool haveEntry = planned && m_CurrentPath.GetCurrentPt(entry);
		const NavFlags actions = F_NAV_CLIMB | F_NAV_JUMP | F_NAV_JUMPLOW | F_NAV_JUMPGAP |
			F_NAV_TELEPORT | F_NAV_ELEVATOR | F_NAV_SCRIPT | F_NAV_DOOR;
		const bool ordinaryEntry = haveEntry && entry.m_NavId != 0 &&
			!(entry.m_NavFlags & actions) && !entry.m_TceWalkDrop && !entry.m_TceStandJump &&
			!entry.m_TceFreeClimb && !entry.m_OnPathThrough;
		const bool differentEntry = ordinaryEntry &&
			(entry.m_NavId != blocked.m_NavId || SquaredLength(RawWpPos(entry),RawWpPos(blocked)) > 1.f);
		const bool usable = differentEntry &&
			GetClient()->CanWalkTo(RawWpPos(entry),entry.m_NavFlags);
		if(!usable)
		{
			const Vector3f proposed = haveEntry ? RawWpPos(entry) : Vector3f::ZERO;
			const Vector3f goal = destinations[destinationIndex].m_Position;
			const char *reason = !planned ? "no_native_path" : !haveEntry ? "empty_native_path" :
				entry.m_NavId == 0 ? "final_volume_without_entry" : !ordinaryEntry ? "action_entry" :
				!differentEntry ? "same_blocked_entry" : "entry_walk_rejected";
			EngineFuncs::ConsoleMessage(va("TCE2PATH REENTRY_REJECT bot=%d old=%d attempt=%d proposed=%d proposed_pos=(%.3f %.3f %.3f) reason=%s destination=(%.3f %.3f %.3f) radius=%.3f",
				GetClient()->GetGameID(),blocked.m_NavId,m_TceReentryAttempts,haveEntry ? entry.m_NavId : 0,
				proposed.x,proposed.y,proposed.z,reason,goal.x,goal.y,goal.z,destinations[destinationIndex].m_Radius));
			m_CurrentPath = oldPath;
			m_PathStatus = oldStatus;
			return false;
		}
		m_TceReentryPosition = GetClient()->GetPosition();
		m_TceReentryPendingProgress = true;
		if(m_TceReentryBestRemaining < 0.f)
		{
			m_TceReentryBudgetPosition = m_TceReentryPosition;
			m_TceReentryDestination = destinations[destinationIndex].m_Position;
			m_TceReentryBestRemaining = Length(m_TceReentryPosition,m_TceReentryDestination);
			m_TceReentryCheckpointId = entry.m_NavId;
		}
		const Vector3f target = RawWpPos(entry);
		const Vector3f goal = destinations[destinationIndex].m_Position;
		EngineFuncs::ConsoleMessage(va("TCE2PATH REENTRY bot=%d user=%s old=%d entry=%d attempt=%d pos=(%.3f %.3f %.3f) target=(%.3f %.3f %.3f) destination=(%.3f %.3f %.3f) radius=%.3f",
			GetClient()->GetGameID(),Utils::HashToString(m_Query.m_User->GetFollowUserName()).c_str(),blocked.m_NavId,entry.m_NavId,m_TceReentryAttempts,
			m_TceReentryPosition.x,m_TceReentryPosition.y,m_TceReentryPosition.z,target.x,target.y,target.z,
			goal.x,goal.y,goal.z,destinations[destinationIndex].m_Radius));
		GetClient()->GetSteeringSystem()->SetTarget(target,MinT(entry.m_Radius,1.f),m_Query.m_MoveMode,false);
		return true;
	}

	void FollowPath::ReplenishTceReentryBudget(const Path::PathPoint &reached)
	{
		// Called only after normal arrival and the fresh outgoing walk proof.
		// Sideways combat motion, repeated planning and circles cannot renew
		// attempts: the same original destination must become at least 128u
		// closer than the last accepted (never increasing) distance checkpoint.
		if(!GetClient()->UseValidatedWalkEntries() || m_TceReentryAttempts <= 0 ||
			m_TceReentryBestRemaining < 0.f || !m_Query.m_User || m_PassThroughState ||
			m_TceDoorPhase || reached.m_NavId == 0 || reached.m_NavId == m_TceReentryCheckpointId ||
			(reached.m_NavFlags & ~(F_NAV_CROUCH | F_NAV_PRONE)) || reached.m_TceWalkDrop ||
			reached.m_TceStandJump || reached.m_TceFreeClimb || reached.m_OnPathThrough ||
			!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) || GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER)) return;
		Path::PathPoint next;
		if(!m_CurrentPath.GetNextPt(next) ||
			(next.m_NavFlags & ~(F_NAV_CROUCH | F_NAV_PRONE)) || next.m_TceWalkDrop ||
			next.m_TceStandJump || next.m_TceFreeClimb || next.m_OnPathThrough) return;
		const int index = m_Query.m_User->m_DestinationIndex;
		if(index < 0 || index >= (int)m_Query.m_Destination.size() ||
			SquaredLength(m_Query.m_Destination[index].m_Position,m_TceReentryDestination) > .0001f) return;
		const Vector3f position = GetClient()->GetPosition();
		const float remaining = Length(position,m_TceReentryDestination);
		if(m_TceReentryBestRemaining - remaining < 128.f ||
			SquaredLength2d(position,m_TceReentryBudgetPosition) < 128.f*128.f) return;
		EngineFuncs::ConsoleMessage(va("TCE2PATH REENTRY_PROGRESS bot=%d waypoint=%d attempts=%d remaining=%.3f previous=%.3f",
			GetClient()->GetGameID(),reached.m_NavId,m_TceReentryAttempts,remaining,m_TceReentryBestRemaining));
		m_TceReentryAttempts = 0;
		m_TceReentryBestRemaining = remaining;
		m_TceReentryBudgetPosition = position;
		m_TceReentryCheckpointId = reached.m_NavId;
	}

	bool FollowPath::WaitForTceGround(const Path::PathPoint &point)
	{
		if(!GetClient()->UseValidatedWalkEntries()) return false;
		const int now = IGame::GetTime();
		if(m_PassThroughState || m_TceDoorPhase ||
			GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER)) return false;
		const Vector3f pos = GetClient()->GetPosition(), velocity = GetClient()->GetVelocity();
		const bool grounded = GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND);
		Vector3f supportedPosition;
		// ONGROUND can mean another player, whose body is deliberately absent
		// from the static navigation sweeps. Do not turn that temporary perch
		// into an ordinary path failure or invent a walk down to the floor.
		// Landing needs the unchanged actual full body and fresh safe support.
		const bool supported = grounded && GetClient()->ResolveWalkDestination(pos,.25f,
			point.m_NavFlags,supportedPosition) && SquaredLength(pos,supportedPosition) <= .25f*.25f;
		if(supported)
		{
			if(m_TceAirWaitActive)
				EngineFuncs::ConsoleMessage(va("TCE2PATH AIR_LANDED bot=%d current=%d air_ms=%d ground_supported=1 ground_entity=%d",
					GetClient()->GetGameID(),point.m_NavId,now-m_TceAirWaitStart,
					TceDoorStat(GetClient()->GetGameEntity(),"tce_free_climb_ground")));
			m_TceAirWaitActive = false;
			return false;
		}
		Path::PathPoint next;
		const bool haveNext = m_CurrentPath.GetNextPt(next);
		if(!m_TceAirWaitActive)
		{
			m_TceAirWaitActive = true;
			m_TceAirWaitStart = now;
			EngineFuncs::ConsoleMessage(va("TCE2PATH AIR_WAIT bot=%d current=%d next=%d pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f) ground=%d ground_supported=0 ground_entity=%d",
				GetClient()->GetGameID(),point.m_NavId,haveNext ? next.m_NavId : 0,
				pos.x,pos.y,pos.z,velocity.x,velocity.y,velocity.z,grounded,
				TceDoorStat(GetClient()->GetGameEntity(),"tce_free_climb_ground")));
		}
		GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
		Path::PathPoint previous;
		const bool havePrevious = m_CurrentPath.GetPreviousPt(previous);
		const bool legacyJumpLanding = !grounded && point.m_NavId != 0 &&
			havePrevious && previous.m_NavId != 0 &&
			(previous.m_NavFlags & (F_NAV_JUMP | F_NAV_JUMPLOW | F_NAV_JUMPGAP)) &&
			!previous.m_TceWalkDrop && !previous.m_TceStandJump && !previous.m_TceFreeClimb;
		if(legacyJumpLanding)
		{
			// The accepted legacy jump still needs air control to its actual
			// landing. Hold arrival and the NEXT leg, not this proved flight.
			const Vector3f landing = RawWpPos(point);
			GetClient()->GetSteeringSystem()->SetTarget(landing,0.f,m_Query.m_MoveMode,false);
			m_LookAheadPt = landing;
			m_LookAheadPt.z = GetClient()->GetEyePosition().z;
			GetClient()->GetAimer()->AddAimRequest(Priority::High,this,GetNameHash());
		}
		else GetClient()->GetSteeringSystem()->SetTarget(pos);
		if(now-m_TceAirWaitStart < 750) return true;
		GetClient()->GetSteeringSystem()->SetTarget(pos);
		const Vector3f currentTarget = RawWpPos(point);
		EngineFuncs::ConsoleMessage(va("TCE2PATH BLOCKED_ORDINARY bot=%d current=%d next=%d reason=%s air_ms=%d pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f) ground=%d ladder=0 ground_supported=0 ground_entity=%d stuck_ms=%d reentries=%d user=%s current_radius=%.3f next_radius=%.3f previous=%d previous_flags=%llu destination_volume=%d current_flags=%llu next_flags=%llu current_target=(%.3f %.3f %.3f)",
			GetClient()->GetGameID(),point.m_NavId,haveNext ? next.m_NavId : 0,
			grounded ? "unsupported_ground_timeout" : "airborne_timeout",now-m_TceAirWaitStart,
			pos.x,pos.y,pos.z,velocity.x,velocity.y,velocity.z,grounded,
			TceDoorStat(GetClient()->GetGameEntity(),"tce_free_climb_ground"),GetClient()->GetStuckTime(),m_TceReentryAttempts,
			m_Query.m_User ? Utils::HashToString(m_Query.m_User->GetFollowUserName()).c_str() : "none",
			point.m_Radius,haveNext ? next.m_Radius : 0.f,havePrevious ? previous.m_NavId : 0,
			(unsigned long long)(havePrevious ? previous.m_NavFlags : 0),point.m_TceDestinationVolume,
			(unsigned long long)point.m_NavFlags,(unsigned long long)(haveNext ? next.m_NavFlags : 0),
			currentTarget.x,currentTarget.y,currentTarget.z));
		NotifyUserFailed(FollowPathUser::Blocked);
		m_PathStatus = PathFinished;
		return true;
	}

	void FollowPath::ResetTceOrdinaryLadderContact()
	{
		m_TceOrdinaryLadderStart = 0;
	}

	int FollowPath::UpdateTceOrdinaryLadderContact(const Path::PathPoint &point)
	{
		if(!GetClient()->UseValidatedWalkEntries()) return 0;
		const int now = IGame::GetTime();
		if(m_TceOrdinaryLadderStart && now < m_TceOrdinaryLadderStart)
		{
			Stop(true);
			ResetTceOrdinaryLadderContact();
			return -2;
		}
		const NavFlags actions = F_NAV_CLIMB | F_NAV_JUMP | F_NAV_JUMPLOW | F_NAV_JUMPGAP |
			F_NAV_TELEPORT | F_NAV_ELEVATOR | F_NAV_SCRIPT | F_NAV_DOOR | F_NAV_UNDERWATER;
		Path::PathPoint previous;
		const bool havePrevious = m_CurrentPath.GetPreviousPt(previous);
		if(m_PassThroughState || m_TceDoorPhase || (point.m_NavFlags & actions) ||
			point.m_TceWalkDrop || point.m_TceStandJump || point.m_TceFreeClimb ||
			point.m_TceDoor.marked || point.m_OnPathThrough ||
			(havePrevious && ((previous.m_NavFlags & actions) || previous.m_TceWalkDrop ||
			 previous.m_TceStandJump || previous.m_TceFreeClimb))) return 0;
		const bool ladder = GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER);
		if(!m_TceOrdinaryLadderStart && !ladder) return 0;
		const Vector3f pos = GetClient()->GetPosition();
		Vector3f supported;
		if(GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
			GetClient()->ResolveWalkDestination(pos,.25f,point.m_NavFlags,supported) &&
			SquaredLength(pos,supported) <= .25f*.25f)
		{
			if(m_TceOrdinaryLadderStart)
				EngineFuncs::ConsoleMessage(va("TCE2PATH LADDER_LANDED bot=%d current=%d contact_ms=%d pos=(%.3f %.3f %.3f)",
					GetClient()->GetGameID(),point.m_NavId,now-m_TceOrdinaryLadderStart,pos.x,pos.y,pos.z));
			ResetTceOrdinaryLadderContact();
			m_TceAirWaitActive = false;
			return 0;
		}
		if(!m_TceOrdinaryLadderStart && ladder)
		{
			m_TceOrdinaryLadderStart = now;
			Path::PathPoint next;
			const bool haveNext = m_CurrentPath.GetNextPt(next);
			EngineFuncs::ConsoleMessage(va("TCE2PATH LADDER_CONTACT bot=%d current=%d next=%d pos=(%.3f %.3f %.3f)",
				GetClient()->GetGameID(),point.m_NavId,haveNext ? next.m_NavId : 0,pos.x,pos.y,pos.z));
		}
		if(m_TceOrdinaryLadderStart && now-m_TceOrdinaryLadderStart >= 3000)
		{
			EngineFuncs::ConsoleMessage(va("TCE2PATH BLOCKED_ORDINARY bot=%d current=%d reason=ladder_contact_timeout contact_ms=%d pos=(%.3f %.3f %.3f) ground=%d ladder=%d",
				GetClient()->GetGameID(),point.m_NavId,now-m_TceOrdinaryLadderStart,pos.x,pos.y,pos.z,
				GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND),ladder));
			return -1;
		}
		if(!ladder)
			// Losing real contact returns to the existing bounded fall gate.
			// Neither its timer nor the contact deadline is renewed by flicker.
			return m_TceOrdinaryLadderStart && WaitForTceGround(point) ? 2 : 0;
		return 1;
	}

	void FollowPath::ResetTceDropTrafficLease()
	{
		m_TceDropTrafficStart = m_TceDropTrafficLast = m_TceDropTrafficCredit = 0;
		m_TceDropTrafficNavId = 0;
		m_TceDropTrafficContact = false;
		m_TceDropTrafficPosition = Vector3f::ZERO;
	}

	bool FollowPath::MaintainTceDropTrafficLease(const Path::PathPoint &point)
	{
		if(!m_TceDropTrafficStart) return false;
		if(IGame::GetTime() < m_TceDropTrafficStart)
		{
			// A new engine clock cannot inherit an action from its old phase.
			Stop(true);
			ResetTceDropTrafficLease();
			return true;
		}
		const Vector3f position = GetClient()->GetPosition();
		Vector3f supported;
		// Observe intervening progress on EVERY path, including walking away
		// and later returning to the SAME launch. Replanning at rest is not it.
		if(SquaredLength2d(position,m_TceDropTrafficPosition) >= 128.f*128.f &&
			GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) && !GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER) &&
			GetClient()->ResolveWalkDestination(position,.25f,point.m_NavFlags,supported) &&
			SquaredLength(position,supported) <= .25f*.25f)
			ResetTceDropTrafficLease();
		return false;
	}

	int FollowPath::UpdateTceDropTraffic(const Path::PathPoint &point)
	{
		const int now = IGame::GetTime();
		const Vector3f position = GetClient()->GetPosition();
		Vector3f supported;
		const bool grounded = GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
			!GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER);
		const bool support = grounded && GetClient()->ResolveWalkDestination(position,.25f,
			point.m_NavFlags,supported) && SquaredLength(position,supported) <= .25f*.25f;
		bool contact = false;
		AABB hull;
		if(support && SquaredLength2d(position,RawWpPos(point)) > Mathf::Sqr(MinT(point.m_Radius,2.f)) &&
			GetClient()->CanWalkTo(RawWpPos(point),point.m_NavFlags) &&
			GetClient()->GetNavigationHull(hull,point.m_NavFlags))
		{
			obTraceResult trace;
			BitFlag32 category;
			BitFlag64 flags;
			if(EngineFuncs::TraceLine(trace,position,RawWpPos(point),&hull,
				TR_MASK_SOLID | TR_MASK_PLAYERCLIP | TR_MASK_PLAYER | TR_MASK_NAVCAPSULE,
				GetClient()->GetGameID(),False) && !trace.m_StartSolid && trace.m_Fraction < 1.f &&
				trace.m_HitEntity.IsValid() &&
				InterfaceFuncs::GetEntityCategory(trace.m_HitEntity,category) && category.CheckFlag(ENT_CAT_PLAYER) &&
				GetClient()->GetTeam() > 0 && InterfaceFuncs::GetEntityTeam(trace.m_HitEntity) == GetClient()->GetTeam() &&
				InterfaceFuncs::GetEntityFlags(trace.m_HitEntity,flags) && flags.CheckFlag(ENT_FLAG_ONGROUND) &&
				!flags.CheckFlag(ENT_FLAG_DEAD) && InterfaceFuncs::IsAlive(trace.m_HitEntity) &&
				!flags.CheckFlag(ENT_FLAG_ONLADDER))
			{
				contact = true;
			}
		}
		if(contact && !m_TceDropTrafficStart)
		{
			m_TceDropTrafficStart = now;
			m_TceDropTrafficNavId = point.m_NavId;
			m_TceDropTrafficPosition = position;
			EngineFuncs::ConsoleMessage(va("TCE2WALKDROP TRAFFIC_WAIT bot=%d waypoint=%d budget_ms=4000",
				GetClient()->GetGameID(),point.m_NavId));
		}
		const bool expired = contact && (now < m_TceDropTrafficStart || now-m_TceDropTrafficStart >= 4000);
		if(contact && !expired && m_TceDropTrafficContact && m_TceDropTrafficLast > 0 && now >= m_TceDropTrafficLast)
			m_TceDropTrafficCredit = MinT(4000,m_TceDropTrafficCredit+now-m_TceDropTrafficLast);
		if(!contact && m_TceDropTrafficContact)
			EngineFuncs::ConsoleMessage(va("TCE2WALKDROP TRAFFIC_CLEAR bot=%d waypoint=%d paused_ms=%d",
				GetClient()->GetGameID(),point.m_NavId,m_TceDropTrafficCredit));
		m_TceDropTrafficLast = now;
		m_TceDropTrafficContact = contact;
		if(contact && !expired) GetClient()->ResetStuckTime();
		return expired ? -1 : contact ? 1 : 0;
	}

	State::StateStatus FollowPath::Update(float fDt)
	{
		Prof(FollowPath);
		{
			Prof(Update);
			if(m_TceReentryPendingProgress && GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
				SquaredLength2d(GetClient()->GetPosition(),m_TceReentryPosition) >= 16.f*16.f)
			{
				m_TceReentryPendingProgress = false;
				GetClient()->ResetStuckTime();
			}

			Path::PathPoint pt;
			m_CurrentPath.GetCurrentPt(pt);
			if(MaintainTceDropTrafficLease(pt)) return State_Finished;
			if(m_TceAirWaitActive && GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) && WaitForTceGround(pt))
				return m_PathStatus == PathFinished ? State_Finished : State_Busy;
			if(m_TceCentreNavId && m_TceCentreNavId != pt.m_NavId)
				ResetTceCentreApproach();
			const int doorAction = UpdateTceDoor(pt);
			if(doorAction > 0) return State_Busy;
			if(doorAction < 0)
			{
				GetClient()->GetSteeringSystem()->SetTarget(GetClient()->GetPosition());
				const Vector3f pos = GetClient()->GetPosition(), target = RawWpPos(pt);
				EngineFuncs::ConsoleMessage(va("TCE2DOOR BLOCKED bot=%d phase=%d entry=%d current=%d pos=(%.3f %.3f %.3f) target=(%.3f %.3f %.3f)",
					GetClient()->GetGameID(), m_TceDoorPhase, m_TceDoorEntry.m_NavId, pt.m_NavId,
					pos.x, pos.y, pos.z, target.x, target.y, target.z));
				ResetTceDoor();
				NotifyUserFailed(FollowPathUser::Blocked);
				m_PathStatus = PathFinished;
				return State_Finished;
			}

			GetClient()->ProcessGotoNode(m_CurrentPath);
			const int ordinaryLadder = UpdateTceOrdinaryLadderContact(pt);
			if(ordinaryLadder == -2) return State_Finished;
			if(ordinaryLadder == -1)
			{
				GetClient()->GetSteeringSystem()->SetTarget(GetClient()->GetPosition());
				NotifyUserFailed(FollowPathUser::Blocked);
				m_PathStatus = PathFinished;
				return State_Finished;
			}
			if(ordinaryLadder == 2)
				return m_PathStatus == PathFinished ? State_Finished : State_Busy;
			const bool ordinaryLadderTransit = ordinaryLadder == 1;

			bool b3dDistanceCheck = ordinaryLadderTransit || (pt.m_NavFlags & F_NAV_ELEVATOR) || (pt.m_NavFlags & F_NAV_CLIMB);
			bool b3dMovement = b3dDistanceCheck;
			bool bConsumeWaypoint = !ordinaryLadderTransit;
			float steeringRadius = ordinaryLadderTransit ? 0.f : pt.m_Radius;
			float steeringInputScale = 1.f;

			const Vector3f vPos = GetClient()->GetPosition();
			Vector3f vGotoTarget = RawWpPos(pt);
			const bool tceDestination = GetClient()->UseValidatedWalkEntries() &&
				pt.m_TceDestinationVolume && m_CurrentPath.IsEndOfPath() && pt.m_NavId == 0 &&
				!(pt.m_NavFlags & ~(F_NAV_CROUCH | F_NAV_PRONE));
			float destinationRadius = pt.m_Radius;
			if(tceDestination && !ordinaryLadderTransit && WaitForTceGround(pt))
				return m_PathStatus == PathFinished ? State_Finished : State_Busy;
			if(tceDestination && !ordinaryLadderTransit)
			{
				if(!GetClient()->ResolveWalkDestination(RawWpPos(pt), pt.m_Radius, pt.m_NavFlags, vGotoTarget))
				{
					EngineFuncs::ConsoleMessage(va("TCE2PATH BLOCKED_DESTINATION bot=%d user=%s raw=(%.3f %.3f %.3f) radius=%.3f pos=(%.3f %.3f %.3f)",
						GetClient()->GetGameID(), m_Query.m_User ? Utils::HashToString(m_Query.m_User->GetFollowUserName()).c_str() : "none",
						pt.m_Pt.x,pt.m_Pt.y,pt.m_Pt.z,pt.m_Radius,vPos.x,vPos.y,vPos.z));
					GetClient()->GetSteeringSystem()->SetTarget(vPos);
					NotifyUserFailed(FollowPathUser::Blocked);
					m_PathStatus = PathFinished;
					return State_Finished;
				}
				// PMove may rest a fraction above the queried support. Do not
				// brake inside a larger XY circle while still outside the actual
				// caller's 3D goal sphere at the actor's current height.
				const float zOffset = MaxT(Mathf::FAbs(vGotoTarget.z - pt.m_Pt.z),
					Mathf::FAbs(vPos.z - pt.m_Pt.z));
				destinationRadius = Mathf::Sqrt(MaxT(0.f, pt.m_Radius*pt.m_Radius-zOffset*zOffset));
				steeringRadius = destinationRadius;
				bConsumeWaypoint = GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
					Mathf::FAbs(vPos.z-vGotoTarget.z) <= 2.f &&
					SquaredLength(vPos,pt.m_Pt) <= pt.m_Radius*pt.m_Radius;
			}
			Path::PathPoint previousPt;
			const bool havePreviousPt = m_CurrentPath.GetPreviousPt(previousPt);
			const bool tceAscentExit = GetClient()->UseValidatedWalkEntries() &&
				(pt.m_NavFlags & F_NAV_CLIMB) && havePreviousPt &&
				previousPt.m_TceAscentRelease;
			const bool tceDropLaunch = GetClient()->UseValidatedWalkEntries() && pt.m_TceWalkDrop;
			const bool tceStandLaunch = GetClient()->UseValidatedWalkEntries() && pt.m_TceStandJump;
			const bool tceFreeLaunch = GetClient()->UseValidatedWalkEntries() && pt.m_TceFreeClimb;
			const bool tceFreeLanding = GetClient()->UseValidatedWalkEntries() && havePreviousPt && previousPt.m_TceFreeClimb;
            const bool tceStandLanding = GetClient()->UseValidatedWalkEntries() && havePreviousPt && previousPt.m_TceStandJump;
            const bool tceStandMarked = GetClient()->UseValidatedWalkEntries() && pt.m_TceStandJump;
            const bool tceDropLanding = GetClient()->UseValidatedWalkEntries() &&
				havePreviousPt && previousPt.m_TceWalkDrop;
			GetClient()->GetAimer()->AddAimRequest(
				GetClient()->UseValidatedWalkEntries() &&
				// Ladder contact depends on facing throughout the current climb,
				// including its approach before ONLADDER becomes true. Combat
				// aiming must not turn a checked descent away from its ladder.
				(ordinaryLadderTransit || (pt.m_NavFlags & F_NAV_CLIMB) || tceAscentExit || m_TceDoorPhase == 4 ||
				 (m_TceDoorPhase == 3 && m_CurrentPath.IsEndOfPath() && SquaredLength2d(vPos,m_TceDoorEntry.m_Pt) <= 1024.f) ||
				 pt.m_TceAscentAlignment || pt.m_TceAscentRelease ||
				 tceDropLaunch || tceDropLanding || tceStandLaunch || tceStandLanding || tceFreeLaunch || tceFreeLanding) ?
				Priority::High : Priority::Idle, this, GetNameHash());
			if(tceAscentExit)
			{
				// The marked release starts a ballistic, horizontal dismount.
				// Normalizing its lower landing target in 3D weakens air steering
				// and can make the ladder adapter request a downward remount.
				b3dMovement = false;
				if(m_TceExitNavId != pt.m_NavId)
				{
					m_TceExitNavId = pt.m_NavId;
					m_TceExitStartTime = IGame::GetTime();
				}
				const bool supported = GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
					!GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER) &&
					Mathf::FAbs(vPos.z - vGotoTarget.z) <= 2.f;
				if(!supported)
				{
					bConsumeWaypoint = false;
					Vector3f direction = (vGotoTarget - RawWpPos(previousPt)).Flatten();
					const float exitLength = direction.Normalize();
					const Vector3f fromRelease = (vPos - RawWpPos(previousPt)).Flatten();
					const float along = fromRelease.Dot(direction);
					const float sideSq = MaxT(0.f, fromRelease.SquaredLength() - along * along);
					// Crossing the XY landing coordinate while still airborne must
					// not brake/reverse into the ladder. Continue through its checked
					// approach plane until actual floor contact, with bounded failure
					// if that contact never happens (rather than a zero-input hang).
					if(exitLength < .01f || IGame::GetTime() - m_TceExitStartTime > 2000 ||
						vPos.z < vGotoTarget.z - 18.f || along > exitLength + 24.f || sideSq > 256.f)
					{
						GetClient()->GetSteeringSystem()->SetTarget(vPos);
						NotifyUserFailed(FollowPathUser::Blocked);
						m_PathStatus = PathFinished;
						return State_Finished;
					}
				}
			}
			else m_TceExitStartTime = m_TceExitNavId = 0;
			if(tceDropLanding)
			{
				// Native proof starts with zero vertical impulse. Never reinterpret
				// this low-ceiling descent as JUMP or allow ordinary corner cutting.
				b3dMovement = false;
				GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
				if(m_TceDropNavId != pt.m_NavId)
				{
					m_TceDropNavId = pt.m_NavId;
					m_TceDropStartTime = IGame::GetTime();
				}
				Vector3f direction = (vGotoTarget - RawWpPos(previousPt)).Flatten();
				const float length = direction.Normalize();
				const Vector3f fromLaunch = (vPos - RawWpPos(previousPt)).Flatten();
				const float along = fromLaunch.Dot(direction);
				const float sideSq = MaxT(0.f, fromLaunch.SquaredLength() - along * along);
				// Includes the separately validated floor continuation after early
				// touchdown; landing before its waypoint is ordinary forward walk.
				if(length < .01f || IGame::GetTime() - m_TceDropStartTime > 4000 ||
					vPos.z < vGotoTarget.z - 18.f || along < -8.f || along > length + 48.f || sideSq > 64.f)
				{
					GetClient()->GetSteeringSystem()->SetTarget(vPos);
					NotifyUserFailed(FollowPathUser::Blocked);
					m_PathStatus = PathFinished;
					return State_Finished;
				}
				if(!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND)) {
					// Stay in the proved forward fall corridor. A fast loadout can
					// pass the destination XY before landing; the native receipt
					// proves its bounded touchdown and ordinary return on the floor.
					vGotoTarget=RawWpPos(previousPt)+direction*MaxT(length,along+16.f);
				}
			}
			else m_TceDropStartTime = m_TceDropNavId = 0;

			float fDistanceSq = 0.f;
			const float fDistSq = SquaredLength(vPos, vGotoTarget);
			const float fDistSq2d = SquaredLength2d(vPos, vGotoTarget);
			const float fWpRadiusSq = Mathf::Sqr(destinationRadius);

			if(b3dDistanceCheck)
				fDistanceSq = fDistSq;
			else
				fDistanceSq = fDistSq2d;

			// Handle traversal options.
			if(!pt.m_TceDoor.marked && pt.m_NavFlags & F_NAV_DOOR && IGame::GetFrameNumber() & 3)
				GetClient()->PressButton(BOT_BUTTON_USE);
			if(pt.m_NavFlags & F_NAV_SNEAK)
				GetClient()->PressButton(BOT_BUTTON_WALK);
			if(pt.m_NavFlags & F_NAV_ELEVATOR)
			{
				if(!CheckForMover(pt.m_Pt) && 
					!CheckForMover(GetClient()->GetPosition()))
				{
					GetClient()->ResetStuckTime();

					bConsumeWaypoint = false;
					Path::PathPoint lastpt;
					if(m_CurrentPath.GetPreviousPt(lastpt))
					{
						if(lastpt.m_NavFlags & F_NAV_ELEVATOR)
							vGotoTarget = vPos;
						else
							vGotoTarget = lastpt.m_Pt;
					}
					else
						vGotoTarget = vPos;
				}
				else
				{
					// hack: let it succeed if we're in 2d radius and above the wp
					if(fDistSq2d < fWpRadiusSq && vPos.z > vGotoTarget.z)
					{
						fDistanceSq = fWpRadiusSq;
					}
				}
			}
			if(pt.m_NavFlags & F_NAV_TELEPORT)
			{
				Path::PathPoint nextpt;
				if(m_CurrentPath.GetNextPt(nextpt))
				{
					if(nextpt.m_NavFlags & F_NAV_TELEPORT)
					{
						// have we teleported to the destination teleport?
						const float fDistSqTp = SquaredLength2d(vPos, nextpt.m_Pt);
						if(fDistSqTp < Mathf::Sqr(nextpt.m_Radius))
						{
							fDistanceSq = 0.f;
							bConsumeWaypoint = true;
						}
						else
							bConsumeWaypoint = false;
					}
				}
			}


			// Close enough?
			if(tceDropLaunch || tceDropLanding)
			{
				const float arrival = MinT(pt.m_Radius, tceDropLaunch ? 2.f : 6.f);
				bConsumeWaypoint = bConsumeWaypoint && GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
					!GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER) &&
					Mathf::FAbs(vPos.z - vGotoTarget.z) <= 2.f &&
					fDistSq2d <= arrival * arrival;
				steeringRadius = 0.f;
				GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
				if(bConsumeWaypoint) fDistanceSq = 0.f;
			}
			if(tceDropLaunch)
			{
				// The native drop starts at rest. A radius-only trigger could
				// carry approach momentum sideways over an untested edge. Taper
				// ordinary movement input, then let real ground friction stop us.
				const int now = IGame::GetTime();
				if(m_TceDropLaunchNavId != pt.m_NavId)
				{
					m_TceDropLaunchNavId = pt.m_NavId;
					m_TceDropSettleStart = m_TceDropStableSince = 0;
					m_TceDropTrafficLast = m_TceDropTrafficCredit = 0;
					m_TceDropTrafficContact = false;
				}
				const float distance = Mathf::Sqrt(fDistSq2d);
				steeringInputScale = ClampT(distance / 64.f, .025f, 1.f);
				steeringRadius = MinT(pt.m_Radius, 2.f);
				if(distance <= 8.f && !m_TceDropSettleStart)
				{
					m_TceDropSettleStart = now;
					const Vector3f velocity = GetClient()->GetVelocity();
					EngineFuncs::ConsoleMessage(va("TCE2WALKDROP SETTLE bot=%d waypoint=%d pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f)",
						GetClient()->GetGameID(), pt.m_NavId, vPos.x, vPos.y, vPos.z, velocity.x, velocity.y, velocity.z));
				}
				Path::PathPoint landing;
				Vector3f direction = Vector3f::ZERO;
				if(m_CurrentPath.GetNextPt(landing)) direction = (landing.m_Pt - pt.m_Pt).Flatten();
				Vector3f facing = GetClient()->GetFacingVector().Flatten();
				const bool aligned = direction.Normalize() > 0.f && facing.Normalize() > 0.f &&
					facing.Dot(direction) >= .999f;
				const Vector3f velocity = GetClient()->GetVelocity();
				if(bConsumeWaypoint && velocity.SquaredLength() <= .01f && aligned)
				{
					if(!m_TceDropStableSince) m_TceDropStableSince = now;
					bConsumeWaypoint = now - m_TceDropStableSince >= 100;
				}
				else { m_TceDropStableSince = 0; bConsumeWaypoint = false; }
				const int traffic = m_TceDropSettleStart ?
					UpdateTceDropTraffic(pt) : 0;
				if(m_TceDropSettleStart && (traffic < 0 || now - m_TceDropSettleStart - m_TceDropTrafficCredit > 3000))
				{
					EngineFuncs::ConsoleMessage(va("TCE2WALKDROP SETTLE_FAILED bot=%d waypoint=%d distance=%.3f velocity=(%.3f %.3f %.3f) aligned=%d active_ms=%d traffic_ms=%d reason=%s",
						GetClient()->GetGameID(), pt.m_NavId, distance, velocity.x, velocity.y, velocity.z, aligned ? 1 : 0,
						now-m_TceDropSettleStart-m_TceDropTrafficCredit,m_TceDropTrafficCredit,traffic < 0 ? "traffic_deadline" : "settle_deadline"));
					GetClient()->GetSteeringSystem()->SetTarget(vPos);
					NotifyUserFailed(FollowPathUser::Blocked);
					m_PathStatus = PathFinished;
					return State_Finished;
				}
				if(bConsumeWaypoint)
					EngineFuncs::ConsoleMessage(va("TCE2WALKDROP LAUNCH bot=%d waypoint=%d pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f) stable_ms=%d",
						GetClient()->GetGameID(), pt.m_NavId, vPos.x, vPos.y, vPos.z, velocity.x, velocity.y, velocity.z, now-m_TceDropStableSince));
			}
			else
			{
				m_TceDropLaunchNavId = m_TceDropSettleStart = m_TceDropStableSince = 0;
				m_TceDropTrafficLast = m_TceDropTrafficCredit = 0;
				m_TceDropTrafficContact = false;
			}
            if(tceFreeLaunch || tceFreeLanding)
            {
                Path::PathPoint landing;
                const Vector3f launch = tceFreeLaunch ? RawWpPos(pt) : RawWpPos(previousPt);
                if(tceFreeLaunch && !m_CurrentPath.GetNextPt(landing)) return State_Finished;
                if(tceFreeLanding) landing = pt;
                Vector3f direction=(RawWpPos(landing)-launch).Flatten();
                const float length=direction.Normalize();
                const Vector3f delta=(vPos-launch).Flatten();
                const float along=delta.Dot(direction);
                const float sideSq=MaxT(0.f,delta.SquaredLength()-along*along);
                const bool ground=GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) && !GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER);
                const int now=IGame::GetTime();
                const int nav=tceFreeLaunch?pt.m_NavId:previousPt.m_NavId;
                if(m_TceFreeNavId!=nav){m_TceFreeNavId=nav;m_TceFreePhase=0;m_TceFreeStart=0;}
                // A real dynamic replan can arrive while the engine already
                // reports attached contact in this same proved shaft. Resume
                // forward climbing; do not issue another Space impulse or
                // depend on a stale phase from the discarded path.
                if(tceFreeLaunch&&m_TceFreePhase==0&&GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER)&&
                    vPos.z>launch.z+17.f&&SquaredLength2d(vPos,launch)<=16.f) {
                    m_TceFreePhase=1;m_TceFreeStart=now;
                }
                b3dMovement=false;steeringRadius=0.f;bConsumeWaypoint=false;
                GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
                GetClient()->ReleaseButton(BOT_BUTTON_CROUCH);
                GetClient()->ReleaseButton(BOT_BUTTON_PRONE);
                GetClient()->ReleaseButton(BOT_BUTTON_SPRINT);
                GetClient()->ReleaseButton(BOT_BUTTON_WALK);
                GetClient()->ReleaseButton(BOT_BUTTON_AIM);
                if(!m_TceFreeStart&&SquaredLength2d(vPos,launch)<32.f*32.f)m_TceFreeStart=now;
                // A validated ordinary entry may reach this launch from the
                // side. Centre on its exact point before applying the shaft
                // envelope; that envelope remains strict after Space/contact.
                const bool freeClimbEngaged=tceFreeLanding||m_TceFreePhase!=0;
                if(length<.01f||(freeClimbEngaged&&(sideSq>64.f||vPos.z<launch.z-2.f||along>length+8.f))||
                   (m_TceFreeStart&&now-m_TceFreeStart>(m_TceFreePhase?4000:15000))) {
                    GetClient()->GetSteeringSystem()->SetTarget(vPos);
                    NotifyUserFailed(FollowPathUser::Blocked);m_PathStatus=PathFinished;return State_Finished;
                }
                if(tceFreeLaunch && m_TceFreePhase==0) {
                    vGotoTarget=launch;
                    steeringInputScale=ClampT(Mathf::Sqrt(SquaredLength2d(vPos,launch))/32.f,.025f,1.f);
                    if(ground&&SquaredLength2d(vPos,launch)<=.25f&&Mathf::FAbs(vPos.z-launch.z)<=1.f&&GetClient()->GetVelocity().SquaredLength()<=1.f) {
                        vGotoTarget=vPos;GetClient()->ResetStuckTime();
                        if(TceDoorStat(GetClient()->GetGameEntity(),"tce_free_climb_stamina")>=2500 &&
                            TceDoorStat(GetClient()->GetGameEntity(),"tce_free_climb_ready")==1) {
                            m_TceFreePhase=1;m_TceFreeStart=now;vGotoTarget=RawWpPos(landing);
                            GetClient()->PressButton(BOT_BUTTON_TCE_FREECLIMB);
                            GetClient()->PressButton(BOT_BUTTON_JUMP);
                            EngineFuncs::ConsoleMessage(va("TCE2FREECLIMB LAUNCH bot=%d waypoint=%d",GetClient()->GetGameID(),pt.m_NavId));
                        }
                    }
                } else {
                    vGotoTarget=launch+direction*MaxT(length,along+16.f);
                    if(tceFreeLaunch || !ground)GetClient()->PressButton(BOT_BUTTON_TCE_FREECLIMB);
                    if(tceFreeLaunch && GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER) && vPos.z>launch.z+17.f) {
                        bConsumeWaypoint=true;fDistanceSq=0.f;GetClient()->ResetStuckTime();
                    } else if(tceFreeLanding && ground && Mathf::FAbs(vPos.z-RawWpPos(landing).z)<=2.f) {
                        vGotoTarget=RawWpPos(landing);bConsumeWaypoint=true;
                        fDistanceSq=SquaredLength2d(vPos,vGotoTarget);steeringRadius=MinT(pt.m_Radius,6.f);
                    }
                }
            }
            else {m_TceFreeNavId=m_TceFreePhase=m_TceFreeStart=0;}
            if(tceStandLaunch)
            {
                Path::PathPoint landing;
                if(!m_CurrentPath.GetNextPt(landing)) return State_Finished;
                Vector3f direction=(RawWpPos(landing)-RawWpPos(pt)).Flatten();
                if(direction.Normalize()<=0.f) return State_Finished;
                const int now=IGame::GetTime();
                if(m_TceStandNavId!=pt.m_NavId) {
                    m_TceStandNavId=pt.m_NavId;m_TceStandPhase=0;
                    m_TceStandStart=0;m_TceStandStable=m_TceStandAttempt=0;
                }
                const Vector3f launch=RawWpPos(pt),stop=launch-direction*8.f;
                const Vector3f velocity=GetClient()->GetVelocity();
                const Vector3f offset=(vPos-launch).Flatten();
                const float along=offset.Dot(direction);
                const float sideSq=MaxT(0.f,offset.SquaredLength()-along*along);
                const bool ground=GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
                    !GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER);
                bConsumeWaypoint=false;steeringRadius=0.f;
                GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
                GetClient()->ReleaseButton(BOT_BUTTON_SPRINT);
                GetClient()->ReleaseButton(BOT_BUTTON_WALK);
                GetClient()->ReleaseButton(BOT_BUTTON_CROUCH);
                GetClient()->ReleaseButton(BOT_BUTTON_PRONE);
                GetClient()->ReleaseButton(BOT_BUTTON_LEANLEFT);
                GetClient()->ReleaseButton(BOT_BUTTON_LEANRIGHT);
                GetClient()->ReleaseButton(BOT_BUTTON_AIM);
                if(!m_TceStandStart&&SquaredLength2d(vPos,stop)<64.f*64.f)m_TceStandStart=now;
                // Ordinary predecessor tolerance may leave us beside the
                // precise launch. Centre and brake before enforcing its
                // narrow priming shaft; those bounds remain strict once
                // the physically checked priming sequence has begun.
                if((m_TceStandStart&&now-m_TceStandStart>(m_TceStandPhase?4000:15000)) ||
                    (m_TceStandPhase==1&&(along>3.f||sideSq>64.f||vPos.z<launch.z-2.f))) {
                    EngineFuncs::ConsoleMessage(va("TCE2STANDJUMP FAIL bot=%d waypoint=%d phase=%d reason=launch_band_or_timeout",
                        GetClient()->GetGameID(),pt.m_NavId,m_TceStandPhase));
                    GetClient()->GetSteeringSystem()->SetTarget(vPos);
                    NotifyUserFailed(FollowPathUser::Blocked);m_PathStatus=PathFinished;return State_Finished;
                }
                if(m_TceStandPhase==0) {
                    vGotoTarget=stop;
                    const float stopDistance=Mathf::Sqrt(SquaredLength2d(vPos,stop));
                    steeringInputScale=ClampT(stopDistance/64.f,.025f,1.f);
                    // The priming shaft permits only .5u lateral error at
                    // launch. A 1u braking circle can begin outside that shaft
                    // (the observed diagonal hood approach was .913u aside),
                    // then pass the entire launch band without valid Space.
                    // Centre before priming; keep its physical launch gates.
                    if(stopDistance<=.125f && ground && velocity.SquaredLength()<=1.f) {
                        vGotoTarget=vPos;
                        if(!m_TceStandStable)m_TceStandStable=now;
                        if(now-m_TceStandStable>=100) {m_TceStandPhase=1;m_TceStandStart=now;}
                    } else m_TceStandStable=0;
                } else if(m_TceStandPhase==1) {
                    vGotoTarget=launch+direction*16.f;
                    GetClient()->PressButton(BOT_BUTTON_TCE_STAND_PRIME);
                    const float speedSq=velocity.x*velocity.x+velocity.y*velocity.y;
                    if(ground && along>=-2.f && along<=2.f && sideSq<=.25f &&
                        Mathf::FAbs(vPos.z-launch.z)<=1.f && speedSq>=36.f*36.f &&
                        speedSq<=44.f*44.f && velocity.SquaredLength()<50.f*50.f &&
                        velocity.Flatten().Dot(direction)>35.f) {
                        GetClient()->ReleaseButton(BOT_BUTTON_TCE_STAND_PRIME);
                        GetClient()->PressButton(BOT_BUTTON_TCE_STAND_LAUNCH);
                        m_TceStandPhase=2;m_TceStandAttempt=now;
                        EngineFuncs::ConsoleMessage(va("TCE2STANDJUMP LAUNCH bot=%d waypoint=%d speed=%.3f",
                            GetClient()->GetGameID(),pt.m_NavId,velocity.Length()));
                    }
                } else {
                    vGotoTarget=RawWpPos(landing);
                    GetClient()->PressButton(BOT_BUTTON_TCE_STAND_LAUNCH);
                    if(!ground && velocity.z>0.f) {bConsumeWaypoint=true;fDistanceSq=0.f;}
                    else if(ground&&now-m_TceStandAttempt>100) {
                        GetClient()->ReleaseButton(BOT_BUTTON_TCE_STAND_LAUNCH);
                        m_TceStandPhase=0;m_TceStandStable=0;vGotoTarget=stop;
                    }
                }
            }
            else if(!tceStandLanding) {m_TceStandNavId=m_TceStandPhase=m_TceStandStart=m_TceStandStable=m_TceStandAttempt=0;}
            if(tceStandLanding) {
                const bool ground=GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
                    !GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER);
                const bool supported=ground&&Mathf::FAbs(vPos.z-vGotoTarget.z)<=2.f;
                const int elapsed=IGame::GetTime()-m_TceStandAttempt;
                // A checked arc can touch its intermediate bumper before
                // reaching the hood waypoint. That is not final arrival, nor
                // another jump: prove the remaining grounded walk from the
                // actual actor with the ordinary complete capsule, support,
                // slope and water checks on every continuation frame.
                bool groundContinuation=false;
                const bool needsGroundContinuation=ground && (!supported || fDistSq2d>fWpRadiusSq);
                if(needsGroundContinuation) {
                    Vector3f direction=(RawWpPos(pt)-RawWpPos(previousPt)).Flatten();
                    const float length=direction.Normalize();
                    const Vector3f delta=(vPos-RawWpPos(previousPt)).Flatten();
                    const float along=delta.Dot(direction);
                    const float sideSq=MaxT(0.f,delta.SquaredLength()-along*along);
                    const bool committed=m_TceStandNavId==previousPt.m_NavId &&
                        (m_TceStandPhase==2 || m_TceStandPhase==3);
                    Vector3f currentSupport;
                    groundContinuation=committed && m_TceStandAttempt && elapsed>=0 && elapsed<=2000 &&
                        length>=8.f && length<=128.f && along>=0.f && along<=length+16.f && sideSq<=64.f &&
                        fDistSq2d<=64.f*64.f && vPos.z>RawWpPos(previousPt).z+2.f &&
                        Mathf::FAbs(vPos.z-vGotoTarget.z)<=18.25f &&
                        GetClient()->ResolveWalkDestination(vPos,.25f,pt.m_NavFlags,currentSupport) &&
                        GetClient()->CanWalkTo(vGotoTarget,pt.m_NavFlags);
                    if(groundContinuation) {
                        if(m_TceStandPhase!=3) EngineFuncs::ConsoleMessage(va("TCE2STANDJUMP GROUND_CONTINUE bot=%d waypoint=%d distance=%.3f height=%.3f elapsed=%d",
                            GetClient()->GetGameID(),pt.m_NavId,Mathf::Sqrt(fDistSq2d),vPos.z,elapsed));
                        m_TceStandPhase=3;
                        bConsumeWaypoint=false;steeringRadius=0.f;
                        // A fast ground frame can cross a 2u landing circle.
                        // Brake toward its centre, allowing at most one 16u
                        // end corridor for a freshly proved physical return;
                        // the authored arrival circle itself stays unchanged.
                        steeringInputScale=MinT(steeringInputScale,
                            ClampT(Mathf::Sqrt(fDistSq2d)/32.f,.025f,1.f));
                        GetClient()->ReleaseButton(BOT_BUTTON_TCE_STAND_LAUNCH);
                    }
                }
                GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
                const bool invalidAttempt=!m_TceStandAttempt||elapsed<0||elapsed>2000;
                if(invalidAttempt || (needsGroundContinuation&&!groundContinuation) ||
                    (!supported&&!groundContinuation)) {
                    bConsumeWaypoint=false;steeringRadius=0.f;
                    if(invalidAttempt || m_TceStandPhase==3 || (ground&&elapsed>100)) {
                        EngineFuncs::ConsoleMessage(va("TCE2STANDJUMP FAIL bot=%d waypoint=%d phase=landing reason=unsupported_contact_or_timeout",
                            GetClient()->GetGameID(),pt.m_NavId));
                        GetClient()->GetSteeringSystem()->SetTarget(vPos);
                        NotifyUserFailed(FollowPathUser::Blocked);m_PathStatus=PathFinished;return State_Finished;
                    }
                    GetClient()->PressButton(BOT_BUTTON_TCE_STAND_LAUNCH);
                } else GetClient()->ReleaseButton(BOT_BUTTON_TCE_STAND_LAUNCH);
            }

			if(GetClient()->UseValidatedWalkEntries() && pt.m_TceAscentAlignment &&
				(pt.m_NavFlags & F_NAV_CLIMB))
			{
				// Shift along the upper ladder while still climbing into its
				// actual surface normal. A flat/negative target vector here would
				// disengage mounting as soon as the alignment height is crossed.
				bConsumeWaypoint = fDistSq2d <= fWpRadiusSq && vPos.z >= vGotoTarget.z &&
					GetClient()->GetVelocity().z > 0.f;
				vGotoTarget.z = MaxT(vGotoTarget.z, vPos.z + MaxT(6.f, Mathf::Sqrt(fDistSq2d) * 1.5f));
				steeringRadius = 0.f;
				if(bConsumeWaypoint) fDistanceSq = 0.f;
			}
			if(GetClient()->UseValidatedWalkEntries() && pt.m_TceAscentRelease &&
				(pt.m_NavFlags & F_NAV_CLIMB))
			{
				// Generated ballistic dismounts start at this height while rising.
				// A sphere would consume it radius units too early; a tiny sphere
				// can miss an entire simulation tick and then trigger on descent.
				bConsumeWaypoint = fDistSq2d <= fWpRadiusSq && vPos.z >= vGotoTarget.z &&
					GetClient()->GetVelocity().z > 0.f;
				// Once aligned inside the shaft guide, keep the ascent vertical.
				// A tiny remaining Z delta must not turn a harmless XY offset
				// into a full-speed sideways impulse just before leaving contact.
				if(fDistSq2d <= fWpRadiusSq)
				{
					vGotoTarget.x = vPos.x;
					vGotoTarget.y = vPos.y;
				}
				steeringRadius = 0.f;
				if(bConsumeWaypoint) fDistanceSq = 0.f;
			}
			// Action controllers above must first establish their physical arrival.
			// Ground ownership belongs at the actual ordinary arrival gate, not
			// to guessed predecessor flags. In particular a consumed plain JUMP
			// has no separate landing controller, while generated Drop/Stand/Free
			// controllers above retain bConsumeWaypoint=false during their flight.
			const NavFlags arrivalActions = F_NAV_CLIMB | F_NAV_JUMP | F_NAV_JUMPLOW |
				F_NAV_JUMPGAP | F_NAV_TELEPORT | F_NAV_ELEVATOR | F_NAV_SCRIPT | F_NAV_DOOR | F_NAV_UNDERWATER;
			const bool ordinaryArrival = !(pt.m_NavFlags & arrivalActions) &&
				!pt.m_TceWalkDrop && !pt.m_TceStandJump && !pt.m_TceFreeClimb && !pt.m_OnPathThrough;
			if(bConsumeWaypoint && ordinaryArrival &&
				(fDistanceSq <= fWpRadiusSq || m_TceAirWaitActive) && WaitForTceGround(pt))
				return m_PathStatus == PathFinished ? State_Finished : State_Busy;
			// Action controllers above must first establish their physical arrival.
			// Only then prove the ordinary outgoing leg; an airborne landing is
			// not an ordinary corner merely because its XY radius was entered.
			// A waypoint radius is not permission to cut an untested corner.
			// TC's narrow door routes can touch an obstacle even with radius 8.
			// Continue to the centre when the actual position cannot walk to the
			// next node. Action transitions have their own traversal semantics.
			if(GetClient()->UseValidatedWalkEntries() && !tceDropLaunch && bConsumeWaypoint &&
				fDistanceSq <= fWpRadiusSq)
			{
				Path::PathPoint nextpt;
				NavFlags actions = F_NAV_CLIMB | F_NAV_JUMP | F_NAV_JUMPLOW |
					F_NAV_JUMPGAP | F_NAV_TELEPORT | F_NAV_ELEVATOR | F_NAV_SCRIPT | F_NAV_DOOR;
				const bool haveNext = m_CurrentPath.GetNextPt(nextpt);
				const bool doorTransition = haveNext && pt.m_TceDoor.Valid() && pt.m_TceDoor.role == 2 &&
					pt.m_TceDoor.target == nextpt.m_NavId;
				// Generated DOOR flags describe precisely one passage edge. Their
				// ordinary approaches/retreats still need the usual live walk gate.
				if(pt.m_TceDoor.marked || (haveNext && nextpt.m_TceDoor.marked)) actions &= ~F_NAV_DOOR;
				Vector3f nextSupported = RawWpPos(nextpt);
				const bool nextDestination = haveNext && nextpt.m_TceDestinationVolume && nextpt.m_NavId == 0 &&
					m_CurrentPath.GetCurrentPtIndex()+2 == m_CurrentPath.GetNumPts() &&
					!(nextpt.m_NavFlags & ~(F_NAV_CROUCH | F_NAV_PRONE));
				const bool ordinaryNext = haveNext && !doorTransition && !((pt.m_NavFlags | nextpt.m_NavFlags) & actions);
				// A marked door's ordinary retreat is still a walk. Use the
				// same ground gate here after its exact transition is classified.
				if(ordinaryNext && WaitForTceGround(pt))
					return m_PathStatus == PathFinished ? State_Finished : State_Busy;
				const bool nextWalk = !ordinaryNext ||
					(nextDestination ? GetClient()->ResolveWalkDestination(RawWpPos(nextpt), nextpt.m_Radius,
						pt.m_NavFlags | nextpt.m_NavFlags, nextSupported) :
						GetClient()->CanWalkTo(RawWpPos(nextpt), pt.m_NavFlags | nextpt.m_NavFlags));
				if(ordinaryNext && !nextWalk)
				{
					// Being 1.5 units from a corner is not reaching its centre:
					// the next capsule sweep can still clip the adjacent wall.
					// Only approach the actual centre over a freshly proved walk,
					// then retry the next leg; no centre arrival exempts that gate.
					const bool centreWalk = GetClient()->CanWalkTo(RawWpPos(pt), pt.m_NavFlags);
					const int now = IGame::GetTime();
					if(m_TceCentreNavId && m_TceCentreNextId != nextpt.m_NavId)
						ResetTceCentreApproach();
					if(centreWalk && !m_TceCentreNavId && fDistSq2d <= 1.5f*1.5f)
					{
						m_TceCentreNavId = pt.m_NavId;
						m_TceCentreNextId = nextpt.m_NavId;
						m_TceCentreStart = now;
						m_TceCentreLastProgress = now;
						m_TceCentreBestDistance = Mathf::Sqrt(fDistSq2d);
						EngineFuncs::ConsoleMessage(va("TCE2PATH CENTRE_APPROACH bot=%d current=%d next=%d distance=%.3f",
							GetClient()->GetGameID(), pt.m_NavId, nextpt.m_NavId, Mathf::Sqrt(fDistSq2d)));
					}
					bool settled = false, exhausted = false;
					if(m_TceCentreNavId)
					{
						++m_TceCentreAttempts;
						const int elapsed = now - m_TceCentreStart;
						const float distance = Mathf::Sqrt(fDistSq2d);
						// Residual approach velocity can first carry the actor away
						// while braking. After this short grace, only a new .125u
						// improvement of the best distance renews the progress clock.
						// Oscillation and merely replanning are not progress.
						if(elapsed <= 250 || distance <= m_TceCentreBestDistance - .125f)
						{
							m_TceCentreBestDistance = distance;
							m_TceCentreLastProgress = now;
						}
						const bool stillAtCentre = fDistSq2d <= .01f*.01f &&
							Mathf::FAbs(vPos.z - RawWpPos(pt).z) <= 2.f &&
							GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND) &&
							GetClient()->GetVelocity().SquaredLength() <= 1.f;
						if(!stillAtCentre) m_TceCentreStillSince = 0;
						else if(!m_TceCentreStillSince) m_TceCentreStillSince = now;
						settled = m_TceCentreStillSince && now - m_TceCentreStillSince >= 100;
						exhausted = elapsed >= 3000 || m_TceCentreAttempts >= 64 ||
							(elapsed >= 1000 && now - m_TceCentreLastProgress >= 1000);
					}
					if(!centreWalk || settled || exhausted)
					{
						if(!centreWalk && fDistSq2d > 4.f*4.f && TryTceReentry(pt)) return State_Busy;
						const Vector3f nextPosition = RawWpPos(nextpt);
						const Vector3f currentTarget = RawWpPos(pt);
						const Vector3f velocity = GetClient()->GetVelocity();
						EngineFuncs::ConsoleMessage(va("TCE2PATH BLOCKED_ORDINARY bot=%d current=%d next=%d pos=(%.3f %.3f %.3f) target=(%.3f %.3f %.3f) reason=%s centre_distance=%.3f centre_ms=%d attempts=%d ground=%d ladder=%d velocity=(%.3f %.3f %.3f) stuck_ms=%d reentries=%d user=%s current_radius=%.3f next_radius=%.3f previous=%d previous_flags=%llu destination_volume=%d current_flags=%llu next_flags=%llu current_target=(%.3f %.3f %.3f)",
							GetClient()->GetGameID(), pt.m_NavId, nextpt.m_NavId, vPos.x, vPos.y, vPos.z,
							nextPosition.x, nextPosition.y, nextPosition.z,
							!centreWalk ? "centre_unreachable" : settled ? "centre_settled" : "centre_timeout",
							Mathf::Sqrt(fDistSq2d), m_TceCentreNavId ? now-m_TceCentreStart : 0, m_TceCentreAttempts,
							GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND),GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER),
							velocity.x,velocity.y,velocity.z,GetClient()->GetStuckTime(),m_TceReentryAttempts,
							m_Query.m_User ? Utils::HashToString(m_Query.m_User->GetFollowUserName()).c_str() : "none",
							pt.m_Radius,nextpt.m_Radius,havePreviousPt ? previousPt.m_NavId : 0,
							(unsigned long long)(havePreviousPt ? previousPt.m_NavFlags : 0),nextDestination,
							(unsigned long long)pt.m_NavFlags,(unsigned long long)nextpt.m_NavFlags,
							currentTarget.x,currentTarget.y,currentTarget.z));
						GetClient()->GetSteeringSystem()->SetTarget(vPos);
						NotifyUserFailed(FollowPathUser::Blocked);
						m_PathStatus = PathFinished;
						ResetTceCentreApproach();
						return State_Finished;
					}
					bConsumeWaypoint = false;
					// Only the fresh outgoing sweep can finish this correction.
					// A .125u steering dead zone stopped an otherwise reachable
					// corner .111u early, leaving its next capsule leg blocked.
					steeringRadius = m_TceCentreNavId ? 0.f : 1.f;
					if(m_TceCentreNavId)
						// Usercmd truncates two facing-relative components to signed
						// chars. Keep at least one nonzero at every yaw, even with
						// WALK's scale64; a .01 input vanished at diagonal facing.
						steeringInputScale = ClampT(Mathf::Sqrt(fDistSq2d) / 32.f, 1.5f/64.f, .125f);
				}
				else if(m_TceCentreNavId)
				{
					EngineFuncs::ConsoleMessage(va("TCE2PATH CENTRE_CLEAR bot=%d current=%d next=%d distance=%.3f centre_ms=%d attempts=%d",
						GetClient()->GetGameID(), pt.m_NavId, haveNext ? nextpt.m_NavId : 0,
						Mathf::Sqrt(fDistSq2d), IGame::GetTime()-m_TceCentreStart, m_TceCentreAttempts));
					ResetTceCentreApproach();
				}
				if(nextDestination && ordinaryNext && nextWalk && bConsumeWaypoint && Mathf::FAbs(nextSupported.z-nextpt.m_Pt.z)>2.f)
					EngineFuncs::ConsoleMessage(va("TCE2PATH DESTINATION_VOLUME bot=%d user=%s raw=(%.3f %.3f %.3f) supported=(%.3f %.3f %.3f) radius=%.3f",
						GetClient()->GetGameID(), m_Query.m_User ? Utils::HashToString(m_Query.m_User->GetFollowUserName()).c_str() : "none",
						nextpt.m_Pt.x,nextpt.m_Pt.y,nextpt.m_Pt.z,nextSupported.x,nextSupported.y,nextSupported.z,nextpt.m_Radius));
			}


			if(bConsumeWaypoint && fDistanceSq <= fWpRadiusSq)
			{
				// Only a physically completed drop can replenish a nearby lease.
				// A replan at its launch, or a different blocking player, cannot.
				if(tceDropLanding && m_TceDropTrafficStart &&
					SquaredLength2d(vPos,m_TceDropTrafficPosition) >= 16.f*16.f)
					m_TceDropTrafficStart = m_TceDropTrafficNavId = 0;
                if(tceStandLanding) EngineFuncs::ConsoleMessage(va("TCE2STANDJUMP LAND bot=%d waypoint=%d height=%.3f",
                    GetClient()->GetGameID(),pt.m_NavId,vPos.z));
				//////////////////////////////////////////////////////////////////////////
				if(!m_PassThroughState && pt.m_OnPathThrough && pt.m_OnPathThroughParam)
				{
					State *pPathThrough = GetClient()->GetLowLevel()->FindState(pt.m_OnPathThrough);
					if(pPathThrough)
					{
						String s = Utils::HashToString(pt.m_OnPathThroughParam);
						if(pPathThrough->OnPathThrough(s))
						{
							m_SavedQuery.m_User = 0;
							m_PassThroughState = pt.m_OnPathThrough;
							m_PathThroughPtIndex = m_CurrentPath.GetCurrentPtIndex();
						}
					}
				}
				//////////////////////////////////////////////////////////////////////////

				if(m_CurrentPath.IsEndOfPath())
				{
					if(!m_Query.m_Final)
					{
						bool bSkipLast = false, bFinalDest = false;
						DestinationVector destlist;
						if(m_Query.m_User && m_Query.m_User->GetNextDestination(destlist, bFinalDest, bSkipLast))
						{
							if(Goto(m_Query.m_User, destlist, m_Query.m_MoveMode, bSkipLast, bFinalDest))
							{
								return State_Busy;
							}
						}
					}

					GetClient()->GetSteeringSystem()->SetTarget(vPos);
					NotifyUserSuccess();
					m_PathStatus = PathFinished;
					return State_Finished;
				}

				ReplenishTceReentryBudget(pt);
				m_CurrentPath.NextPt();
				if(!m_TceReentryPendingProgress) GetClient()->ResetStuckTime();
				if(GetClient()->UseValidatedWalkEntries() && (pt.m_TceAscentRelease || tceDropLaunch || tceStandLaunch || tceFreeLaunch ||
					(m_TceDoorPhase == 4 && pt.m_NavId == m_TceDoorEntry.m_NavId)))
				{
					// Do not steer one more frame back down to the consumed shaft
					// point. Start horizontal exit motion with the upward velocity
					// already acquired from the ladder; gravity handles the descent.
					Path::PathPoint exit;
					if(m_CurrentPath.GetCurrentPt(exit))
					{
						const Vector3f destination = RawWpPos(exit);
						GetClient()->GetSteeringSystem()->SetTarget(destination, MinT(exit.m_Radius, 1.f), m_Query.m_MoveMode, false);
						m_PtOnPath = m_CurrentPath.FindNearestPtOnPath(vPos, &m_LookAheadPt, 128.f);
						m_LookAheadPt = destination;
						m_LookAheadPt.z = GetClient()->GetEyePosition().z;
						if(tceDropLaunch) GetClient()->ReleaseButton(BOT_BUTTON_JUMP);
					}
					return State_Busy;
				}
			}

			if(GetClient()->GetStuckTime() > 2000)
			{
				obint32 lastStuck = m_LastStuckTime;
				m_LastStuckTime = IGame::GetTime();
				if(!m_TceReentryPendingProgress && IGame::GetTime() > lastStuck + 7000) {
					// find new path to current goal
					if(Repath()) return State_Busy;
				}
				// "pathfailed" error
				GetClient()->GetSteeringSystem()->SetTarget(vPos);
				NotifyUserFailed(FollowPathUser::Blocked);
				m_PathStatus = PathFinished;
				return State_Finished;
			}
			
			if(tceAscentExit && !bConsumeWaypoint)
			{
				const Vector3f release = RawWpPos(previousPt);
				Vector3f direction = (vGotoTarget - release).Flatten();
				const float exitLength = direction.Normalize();
				const float along = (vPos - release).Dot(direction);
				vGotoTarget = release + direction * MaxT(exitLength, along + 16.f);
				steeringRadius = 0.f;
			}
			GetClient()->GetSteeringSystem()->SetTarget(vGotoTarget,
				steeringRadius,
				m_Query.m_MoveMode,
				b3dMovement, steeringInputScale);

			//if(pt.m_NavFlags & F_NAV_UNDERWATER)
			//{
				//b3dMovement = true;
				//if(NeedsAir())
			//}

			if(!tceFreeLaunch && !tceFreeLanding && !tceDropLaunch && !tceDropLanding && !tceStandMarked && !tceStandLanding && pt.m_NavFlags & F_NAV_JUMP && (fDistanceSq <= Mathf::Sqr(pt.m_Radius)))
				GetClient()->PressButton(BOT_BUTTON_JUMP);
			if(!tceFreeLaunch && !tceFreeLanding && !tceDropLaunch && !tceDropLanding && !tceStandMarked && !tceStandLanding && pt.m_NavFlags & F_NAV_JUMPLOW)
				CheckForLowJumps(pt.m_Pt);
			if(!tceFreeLaunch && !tceFreeLanding && !tceDropLaunch && !tceDropLanding && !tceStandMarked && !tceStandLanding && pt.m_NavFlags & F_NAV_JUMPGAP)
				CheckForGapJumps(pt.m_Pt);
			if (pt.m_NavFlags & F_NAV_PRONE)
				GetClient()->PressButton(BOT_BUTTON_PRONE);
			else if (pt.m_NavFlags & F_NAV_CROUCH)
				GetClient()->PressButton(BOT_BUTTON_CROUCH);

			m_PtOnPath = m_CurrentPath.FindNearestPtOnPath(vPos, &m_LookAheadPt, 128.f);

			//////////////////////////////////////////////////////////////////////////
			if(tceAscentExit || tceDropLanding || tceStandLanding)
			{
				m_LadderDirection = 0;
				// TC reduces backwards/sideways movement speed. Face the actual
				// landing, not a later turn up to 128 units beyond the small rim.
				m_LookAheadPt = tceDropLanding&&!GetClient()->HasEntityFlag(ENT_FLAG_ONGROUND)?vGotoTarget:RawWpPos(pt);
				m_LookAheadPt.z = GetClient()->GetEyePosition().z;
			}
            else if(tceFreeLaunch || tceFreeLanding) {
                Path::PathPoint landing;
                if(tceFreeLanding)landing=pt;
                else if(!m_CurrentPath.GetNextPt(landing))return State_Finished;
                m_LookAheadPt=RawWpPos(landing);m_LookAheadPt.z=GetClient()->GetEyePosition().z+16.f;
            }
            else if(tceStandLaunch) {
                Path::PathPoint landing;
                if(m_CurrentPath.GetNextPt(landing)) {
                    m_LookAheadPt=RawWpPos(landing);m_LookAheadPt.z=GetClient()->GetEyePosition().z;
                }
            }

			else if(!m_OldLadderStyle)
			{
				if(GetClient()->HasEntityFlag(ENT_FLAG_ONLADDER))
				{
					float fHeight = pt.m_Pt.z - vPos.z;
					
					if(m_LadderDirection == 0)
					{
						if(fDistanceSq < fWpRadiusSq * 4)
						{
							// current waypoint has not been consumed yet
							Path::PathPoint nextpt;
							if(m_CurrentPath.GetNextPt(nextpt))
							{
								fHeight = nextpt.m_Pt.z - vPos.z;
							}
						}
					}

					if(Mathf::FAbs(fHeight) > g_fTopWaypointOffset || m_LadderDirection == 0)
					{
						if (fHeight > 0) m_LadderDirection = 1;
						else m_LadderDirection = -1;
					}
					
					if( 2 * fHeight * fHeight > fDistSq2d //climb only if next waypoint is above or below player
						|| (m_LadderDirection > 0 && fHeight > -g_fTopWaypointOffset)
						|| (m_LadderDirection < 0 && -fHeight > g_fTopWaypointOffset))
					{
						Vector3f vEye = GetClient()->GetEyePosition();
						Vector3f vLook = pt.m_Pt - vEye;
						float h, p, r, cl = Mathf::DegToRad(60.f);

						if(m_LadderDirection > 0)
						{
							GetClient()->PressButton(BOT_BUTTON_MOVEUP);
							vLook.z += g_fTopWaypointOffset;
							vLook.ToSpherical(h, p, r);
							p = ClampT(p,-cl,cl);
						}
						else
						{
							GetClient()->PressButton(BOT_BUTTON_MOVEDN);
							vLook.ToSpherical(h, p, r);
							p = cl;
						}

						vLook.FromSpherical(h, p, r);
						m_LookAheadPt = vEye + vLook;
						//Utils::DrawLine(vEye,m_LookAheadPt, (m_LadderDirection > 0) ? COLOR::GREEN : COLOR::RED,3.f);
					}
				}
				else
				{
					m_LadderDirection = 0;
				}
			}
			//////////////////////////////////////////////////////////////////////////

			// Ignore vertical look points unless it's significantly out of our bbox height.
			const Box3f &worldObb = GetClient()->GetWorldBounds();
			const float fTolerance = worldObb.Extent[2];
			if(Mathf::FAbs(m_LookAheadPt.z - worldObb.Center.z) < fTolerance)
			{
				m_LookAheadPt.z = GetClient()->GetEyePosition().z;
			}
		}
		return State_Busy;
	}

	bool FollowPath::CheckForMover(const Vector3f &_pos)
	{
		const float fWpHeight = g_fTopWaypointOffset - g_fBottomWaypointOffset;

		// Use a ray the size of a waypoint offset down by half a waypoint height.

		Vector3f pos1 = _pos.AddZ(g_fTopWaypointOffset - fWpHeight * 0.5f);
		Vector3f pos2 = pos1.AddZ(-fWpHeight);
		
		const bool bMover = InterfaceFuncs::IsMoverAt(pos1,pos2);

		if(DebugDrawingEnabled())
		{
			Utils::DrawLine(pos1,pos2,bMover?COLOR::GREEN:COLOR::RED,0.5f);
		}		
		return bMover;
	}

	void FollowPath::CheckForLowJumps(const Vector3f &_destination)
	{
		Prof(CheckForLowJumps);

		// bot MUST NOT press jump button every frame, otherwise he jumps only once
		obint32 time = IGame::GetTime();
		if(time - m_JumpTime < 100) return;

		// todo: should be game dependant
		const float fStepHeight = GetClient()->GetStepHeight();
		const float fStepBoxWidth = 9.0f; // GAME DEP
		const float fStepBoxHeight = 16.0f; // GAME DEP
		const float fStepRayLength = 48.0f; // GAME DEP

		// Calculate the vector we're wanting to move, this will be
		// the direction of the trace
		Vector3f vMoveVec = _destination - GetClient()->GetPosition();
		vMoveVec.z = 0.0f; // get rid of the vertical component
		vMoveVec.Normalize();

		// Get this entities bounding box to use for traces.
		AABB worldAABB, localAABB;
		EngineFuncs::EntityWorldAABB(GetClient()->GetGameEntity(), worldAABB);

		// Calculate the local AABB
		const Vector3f &vPosition = GetClient()->GetPosition();

		// Adjust the AABB mins to the step height
		localAABB.m_Mins[2] = worldAABB.m_Mins[2] + fStepHeight - vPosition[2];
		// Adjust the AABB mins to the step height + fStepBoxHeight
		localAABB.m_Maxs[2] = localAABB.m_Mins[2] + fStepBoxHeight;
		// Adjust the mins and max for the width of the box
		localAABB.m_Mins[0] = localAABB.m_Mins[1] = -fStepBoxWidth;
		localAABB.m_Maxs[0] = localAABB.m_Maxs[1] = fStepBoxWidth;

		// Trace a line forward from the bot.
		Vector3f vStartPos;
		worldAABB.CenterPoint(vStartPos);

		// The end of the ray is out towards the move direction, fStepRayLength length
		Vector3f vEndPos(vStartPos + (vMoveVec * fStepRayLength));

		// Trace a line start to end to see if it hits anything.
		obTraceResult tr;
		EngineFuncs::TraceLine(tr, vStartPos, vEndPos, 
			&localAABB, TR_MASK_SOLID | TR_MASK_PLAYERCLIP, GetClient()->GetGameID(), False);

		bool bHit = false;
		if(tr.m_Fraction != 1.0f)
		{
			bHit = true;
			m_JumpTime = time;
			GetClient()->PressButton(BOT_BUTTON_JUMP);
		}
		
		if(DebugDrawingEnabled())
		{
			// Line Ray
			obColor color = bHit ? COLOR::RED : COLOR::GREEN;
			Utils::DrawLine(vStartPos, vEndPos, color, 2.0f);
			Utils::DrawLine(vStartPos.AddZ(localAABB.m_Mins[2]), vEndPos.AddZ(localAABB.m_Mins[2]), color, 2.0f);
			Utils::DrawLine(vStartPos.AddZ(localAABB.m_Maxs[2]), vEndPos.AddZ(localAABB.m_Maxs[2]), color, 2.0f);
		}
	}

	void FollowPath::CheckForGapJumps(const Vector3f &_destination)
	{
		Prof(CheckForGapJumps);

		// Get this entities bounding box to use for traces.
		AABB aabb;
		EngineFuncs::EntityWorldAABB(GetClient()->GetGameEntity(), aabb);

		// Trace a line down from the bot.
		//const float fStartUpOffset = 5.0f;
		static const float fEndDownOffset = GetClient()->GetGameVar(Client::JumpGapOffset);
		Vector3f vStartPos(Vector3f::ZERO);
		aabb.CenterPoint(vStartPos); 

		Vector3f vEndPos(vStartPos);
		vEndPos.z -= fEndDownOffset;

		// Trace a line downward to see the distance below us.
		//DEBUG_ONLY(Utils::DrawLine(vStartPos, vEndPos, COLOR::RED));

		// Trace a line directly underneath us to try and detect rapid drops in elevation.
		obTraceResult tr;
		EngineFuncs::TraceLine(tr, vStartPos, vEndPos, NULL, 
			TR_MASK_SOLID | TR_MASK_PLAYERCLIP, GetClient()->GetGameID(), False);

		Vector3f trEnd(tr.m_Endpos);
		if(m_RayDistance == -1.0f && tr.m_Fraction != 1.0f)
		{
			m_RayDistance = (vStartPos - trEnd).SquaredLength();
		}
		else if(m_RayDistance != -1.0f && tr.m_Fraction == 1.0f)
		{
			GetClient()->PressButton(BOT_BUTTON_JUMP);
		}
		else if(tr.m_Fraction != 1.0f)
		{
			// See if there is any sudden drops. (+20% length in ray maybe?)
			float fNewDist = (vStartPos - trEnd).SquaredLength();
			if(fNewDist > (m_RayDistance + (m_RayDistance * 0.2f)))
			{
				// Try to jump.
				GetClient()->PressButton(BOT_BUTTON_JUMP);
			}
			m_RayDistance = fNewDist;
		}
	}

	//////////////////////////////////////////////////////////////////////////

	Aimer::AimRequest::AimRequest()
	{
		Reset();
	}

	void Aimer::AimRequest::Reset()
	{
		m_Owner = 0;
		m_Priority = Priority::Zero;
		m_AimVector = Vector3f::ZERO;
		m_AimType = WorldPosition;		
		m_AimerUser = NULL;
	}

	Aimer::Aimer() 
		: StateChild("Aimer")
		, m_BestAimOwner(0)
	{
		for(obint32 i = 0; i < MaxAimRequests; ++i)
			m_AimRequests[i].Reset();

		// Initialize default aim request.
		m_AimRequests[0].m_Priority = Priority::Min;
		m_AimRequests[0].m_AimerUser = 0;
		m_AimRequests[0].m_AimVector = Vector3f::UNIT_X;
		m_AimRequests[0].m_AimType = WorldFacing;
		m_AimRequests[0].m_Owner = GetNameHash();
	}

	Aimer::AimRequest *Aimer::FindAimRequest(obuint32 _owner)
	{
		int iOpen = -1;
		for(obint32 i = 0; i < MaxAimRequests; ++i)
		{
			if(m_AimRequests[i].m_Owner == _owner)
			{
				iOpen = i;
				break;
			}
			if(m_AimRequests[i].m_Priority == Priority::Zero)
			{
				if(iOpen == -1)
					iOpen = i;
			}
		}

		if(iOpen != -1)
		{
			return &m_AimRequests[iOpen];
		}
		return NULL;
	}

	bool Aimer::AddAimRequest(Priority::ePriority _prio, AimerUser *_owner, obuint32 _ownername)
	{
		AimRequest *pRequest = FindAimRequest(_ownername);
		if(pRequest)
		{
			pRequest->m_Priority = _prio;
			pRequest->m_Owner = _ownername;
			pRequest->m_AimType = UserCallback;
			pRequest->m_AimerUser = _owner;
			return true;
		}
		return false;
	}

	bool Aimer::AddAimFacingRequest(Priority::ePriority _prio, obuint32 _owner, const Vector3f &_v)
	{
		AimRequest *pRequest = FindAimRequest(_owner);
		if(pRequest)
		{
			pRequest->m_Priority = _prio;
			pRequest->m_Owner = _owner;
			pRequest->m_AimType = WorldFacing;
			pRequest->m_AimerUser = NULL;
			pRequest->m_AimVector = _v;
			return true;
		}
		return false;
	}

	bool Aimer::AddAimPositionRequest(Priority::ePriority _prio, obuint32 _owner, const Vector3f &_v)
	{
		AimRequest *pRequest = FindAimRequest(_owner);
		if(pRequest)
		{
			pRequest->m_Priority = _prio;
			pRequest->m_Owner = _owner;
			pRequest->m_AimType = WorldPosition;
			pRequest->m_AimerUser = NULL;
			pRequest->m_AimVector = _v;
			return true;
		}
		return false;
	}

	bool Aimer::AddAimMoveDirRequest(Priority::ePriority _prio, obuint32 _owner)
	{
		AimRequest *pRequest = FindAimRequest(_owner);
		if(pRequest)
		{
			pRequest->m_Priority = _prio;
			pRequest->m_Owner = _owner;
			pRequest->m_AimType = MoveDirection;
			pRequest->m_AimerUser = NULL;
			return true;
		}
		return false;
	}

	void Aimer::ReleaseAimRequest(obuint32 _owner)
	{
		for(obint32 i = 0; i < MaxAimRequests; ++i)
		{
			if(m_AimRequests[i].m_Owner == _owner)
			{
				m_AimRequests[i].Reset();
				break;
			}
		}
	}

	void Aimer::UpdateAimRequest(obuint32 _owner, const Vector3f &_pos)
	{
		for(obint32 i = 0; i < MaxAimRequests; ++i)
		{
			if(m_AimRequests[i].m_Owner == _owner)
			{
				m_AimRequests[i].m_AimVector = _pos;
				break;
			}
		}
	}

	void Aimer::GetDebugString(StringStr &out)
	{
		out << Utils::HashToString(m_BestAimOwner);
	}

	int Aimer::GetAllRequests(AimRequest *_records, int _max)
	{
		int iNum = 0;
		for(int i = 0; i < MaxAimRequests; ++i)
		{
			if(m_AimRequests[i].m_Priority==Priority::Zero)
				continue;

			_records[iNum++] = m_AimRequests[i];

			if(iNum>=_max-1)
				break;
		}
		return iNum;
	}

	void Aimer::RenderDebug()
	{
		const Aimer::AimRequest *pCurrent = Aimer::GetHighestAimRequest(false);

		for(obint32 i = 1; i < MaxAimRequests; ++i)
		{
			if(m_AimRequests[i].m_Priority > Priority::Zero)
			{
				obColor c = pCurrent==&m_AimRequests[i] ? COLOR::MAGENTA : COLOR::WHITE;
				if(m_AimRequests[i].m_AimType!=WorldFacing)
					Utils::DrawLine(GetClient()->GetEyePosition(),m_AimRequests[i].m_AimVector, c, MIN_RENDER_TIME);
				else
					Utils::DrawLine(GetClient()->GetEyePosition(),GetClient()->GetEyePosition()+GetClient()->GetFacingVector() * 128.f, c, MIN_RENDER_TIME);
			}
		}
	}

	Aimer::AimRequest *Aimer::GetHighestAimRequest(bool _clearontarget)
	{
		int iBestAim = 0;
		for(obint32 i = 1; i < MaxAimRequests; ++i)
		{
			if(m_AimRequests[i].m_Priority > m_AimRequests[iBestAim].m_Priority)
				iBestAim = i;
		}
		return &m_AimRequests[iBestAim];
	}

	void Aimer::OnSpawn()
	{
		m_AimRequests[0].m_AimVector = GetClient()->GetFacingVector();
	}

	void Aimer::Enter()
	{
	}

	void Aimer::Exit()
	{
	}

	State::StateStatus Aimer::Update(float fDt)
	{
		Prof(Aimer);
		{
			Prof(Update);

			AimRequest *curAim = GetHighestAimRequest(true);
			m_BestAimOwner = curAim->m_Owner;
			switch(curAim->m_AimType)
			{
			case WorldPosition:
				{
					GetClient()->TurnTowardPosition(curAim->m_AimVector);
					break;
				}
			case WorldFacing:
				{
					GetClient()->TurnTowardFacing(curAim->m_AimVector);
					break;
				}
			case MoveDirection:
				{
					if(GetClient()->GetFollowPath()->IsActive())
					{
						GetClient()->GetFollowPath()->GetAimPosition(curAim->m_AimVector);
						GetClient()->TurnTowardPosition(curAim->m_AimVector);
						break;
					}
					curAim->m_AimVector = GetClient()->GetSteeringSystem()->GetTarget();
					curAim->m_AimVector.z = GetClient()->GetEyePosition().z;
					GetClient()->TurnTowardPosition(curAim->m_AimVector);
					break;
				}
			case UserCallback:
				{
					Prof(UserCallback);
					OBASSERT(curAim->m_AimerUser, "No Aim User");
					if(curAim->m_AimerUser && 
						curAim->m_AimerUser->GetAimPosition(curAim->m_AimVector) &&
						GetClient()->TurnTowardPosition(curAim->m_AimVector))
					{
						curAim->m_AimerUser->OnTarget();
					}
					break;
				}
			}
		}
		return State_Busy;
	}

	//////////////////////////////////////////////////////////////////////////

	LookAround::LookAround() 
		: StateChild("LookAround")
		, m_NextLookTime(0)
	{
	}

	int LookAround::GetNextLookTime()
	{
		return IGame::GetTime()+Utils::SecondsToMilliseconds(Mathf::IntervalRandom(5.f,15.f));
	}

	void LookAround::OnSpawn()
	{
		m_NextLookTime = GetNextLookTime();
	}

	obReal LookAround::GetPriority()
	{
		if(IGame::GetTime() > m_NextLookTime)
		{
			FollowPath *fp = GetClient()->GetFollowPath();
			Path::PathPoint pp;
			if(fp->IsMoving() && fp->GetCurrentPath().GetCurrentPt(pp) && 
				(pp.m_NavFlags & (F_NAV_CLIMB|F_NAV_DOOR) ))
			{
				m_NextLookTime = GetNextLookTime();
				return 0.f;
			}
			return 1.f;
		}
		return 0.0f;
	}

	void LookAround::Enter()
	{
		//Quaternionf q(Vector3f::UNIT_Z, Mathf::IntervalRandom(-Mathf::PI, Mathf::PI));
		//Vector3f vStartAim = q.Rotate(GetClient()->GetFacingVector());
		GetClient()->GetAimer()->AddAimFacingRequest(Priority::VeryLow, GetNameHash(), -GetClient()->GetFacingVector());
	}

	void LookAround::Exit()
	{
		GetClient()->GetAimer()->ReleaseAimRequest(GetNameHash());
		m_NextLookTime = GetNextLookTime();
	}

	State::StateStatus LookAround::Update(float fDt)
	{
		return (GetStateTime() > 1.f) ? State_Finished : State_Busy;
	}

	//////////////////////////////////////////////////////////////////////////

	MotorControl::MotorControl() 
		: StateSimultaneous("MotorControl") 
	{
		AppendState(new FollowPath);
		AppendState(new SteeringSystem);
		AppendState(new Aimer);
		AppendState(new LookAround);
	}

	//////////////////////////////////////////////////////////////////////////

	LowLevel::LowLevel() 
		: StateSimultaneous("LowLevel") 
	{
		AppendState(new MotorControl);
		AppendState(new WeaponSystem);
		AppendState(new TargetingSystem);
		AppendState(new SensoryMemory);
		AppendState(new ProximityWatcher);
		//AppendState(new FloodFiller);
	}

	//////////////////////////////////////////////////////////////////////////

	Main::Main() 
		: StateSimultaneous("Main"),
		m_OnSpawnCalled(false)
	{
		AppendState(new LowLevel);
		AppendState(new HighLevel);		
	}

	obReal Main::GetPriority()
	{
		if(IGame::GetGameState() != IGame::GetLastGameState())
			return 0.f;

		//return StateSimultaneous::GetPriority();
		return 1.0f;
	}

	void Main::Enter()
	{
		if(!m_OnSpawnCalled)
			GetRootState()->OnSpawn();
		m_OnSpawnCalled = false;
		GetClient()->SendEvent(MessageHelper(MESSAGE_SPAWN));
	}

	void Main::OnSpawn()
	{
		m_OnSpawnCalled = true;
		State::OnSpawn();
	}

	//////////////////////////////////////////////////////////////////////////

	Dead::Dead() 
		: StateChild("Dead")
		, bForceActivate(true)
	{
	}

	obReal Dead::GetPriority() 
	{
		if(bForceActivate)
		{
			bForceActivate = false;
			return 1.f;
		}
		return !InterfaceFuncs::IsAlive(GetClient()->GetGameEntity()) ? 1.f : 0.f; 
	}

	State::StateStatus Dead::Update(float fDt) 
	{
		if(IGame::GetFrameNumber() & 2)
		{
			GetClient()->PressButton(BOT_BUTTON_RESPAWN);
		}
		GetClient()->SetMovementVector(Vector3f::ZERO);
		return State_Busy; 
	}

	//////////////////////////////////////////////////////////////////////////
	
	Warmup::Warmup() 
		: StateChild("Warmup") 
	{
	}

	obReal Warmup::GetPriority() 
	{
		GameState gs = InterfaceFuncs::GetGameState();		
		return (gs != GAME_STATE_PLAYING && gs != GAME_STATE_SUDDENDEATH) ? 1.f : 0.f;		
	}

	State::StateStatus Warmup::Update(float fDt) 
	{
		// need to do something special here?
		return State_Busy; 
	}

	//////////////////////////////////////////////////////////////////////////

	Root::Root() : StateFirstAvailable("Root") 
	{
		AppendState(new Dead);
		AppendState(new Main);
	}

	//////////////////////////////////////////////////////////////////////////

	ProximityWatcher::ProximityWatcher()
		: StateChild("ProximityWatcher")
	{
		for(int i = 0; i < MaxTriggers; ++i)
		{
			m_Triggers[i].m_OwnerState = 0;
			m_Triggers[i].m_DeleteOnFire = true;
		}
	}
	void ProximityWatcher::RenderDebug()
	{
		for(int i = 0; i < MaxTriggers; ++i)
		{
			if(m_Triggers[i].m_SensoryFilter)
			{
				for(int p = 0; p < m_Triggers[i].m_SensoryFilter->GetNumPositions(); ++p)
				{
					float r = Mathf::Max(m_Triggers[i].m_SensoryFilter->GetMaxDistance(), 10.f);
					Utils::DrawRadius(
						m_Triggers[i].m_SensoryFilter->GetPosition(p),
						r, 
						COLOR::MAGENTA, 
						MIN_RENDER_TIME);
				}
			}
		}
	}
	void ProximityWatcher::AddWatch(obuint32 _owner, FilterPtr _filter, bool _fireonce)
	{
		for(int i = 0; i < MaxTriggers; ++i)
		{
			if(!m_Triggers[i].m_SensoryFilter)
			{
				m_Triggers[i].m_OwnerState = _owner;
				m_Triggers[i].m_SensoryFilter = _filter;
				m_Triggers[i].m_DeleteOnFire = _fireonce;
			}
		}
	}
	void ProximityWatcher::RemoveWatch(obuint32 _owner)
	{
		for(int i = 0; i < MaxTriggers; ++i)
		{
			if(m_Triggers[i].m_SensoryFilter && m_Triggers[i].m_OwnerState == _owner)
			{
				m_Triggers[i].m_OwnerState = 0;
				m_Triggers[i].m_SensoryFilter.reset();
				m_Triggers[i].m_DeleteOnFire = false;
			}
		}
	}

	obReal ProximityWatcher::GetPriority()
	{
		for(int i = 0; i < MaxTriggers; ++i)
		{
			if(m_Triggers[i].m_SensoryFilter)
				return 1.f;
		}
		return 0.f;
	}

	State::StateStatus ProximityWatcher::Update(float fDt)
	{
		SensoryMemory *sensory = GetClient()->GetSensoryMemory();

		for(int i = 0; i < MaxTriggers; ++i)
		{
			if(m_Triggers[i].m_SensoryFilter)
			{
				m_Triggers[i].m_SensoryFilter->Reset();

				// Set up ignore list so we don't get events every frame from
				// entities that are still inside the trigger
				// basically only want the event when they enter.
				//m_Triggers[i].m_SensoryFilter->ResetIgnoreEntity();

				sensory->QueryMemory(*m_Triggers[i].m_SensoryFilter);

				if(m_Triggers[i].m_SensoryFilter->DetectedSomething())
				{
					Event_ProximityTrigger trig;
					trig.m_OwnerState = m_Triggers[i].m_OwnerState;
					trig.m_Entity = m_Triggers[i].m_SensoryFilter->GetBestEntity();
					trig.m_Position = m_Triggers[i].m_SensoryFilter->GetTriggerPosition();
					GetClient()->SendEvent(MessageHelper(MESSAGE_PROXIMITY_TRIGGER, &trig, sizeof(trig)));

					if(m_Triggers[i].m_DeleteOnFire)
					{
						m_Triggers[i].m_OwnerState = 0;
						m_Triggers[i].m_SensoryFilter.reset();
						m_Triggers[i].m_DeleteOnFire = false;
					}
				}
			}
		}
		return State_Busy;
	}

	//////////////////////////////////////////////////////////////////////////

	//TrackTargetZone::TrackTargetZone()
	//	: m_Radius(0.f)
	//{
	//	Restart(0.f);
	//}
	//void TrackTargetZone::Restart(float _radius)
	//{
	//	m_Radius = _radius;
	//	m_ValidAim = false;

	//	m_LastTarget.Reset();
	//	for(int i = 0; i < MaxTargetZones; ++i)
	//	{
	//		m_TargetZones[i].m_InUse = false;
	//		m_TargetZones[i].m_TargetCount = 0;
	//		m_TargetZones[i].m_Position = Vector3f::ZERO;
	//	}
	//}

	//void TrackTargetZone::UpdateAimPosition()
	//{
	//	int iNumZones = 0;
	//	float fTotalWeight = 0.f;
	//	for(int i = 0; i < MaxTargetZones; ++i)
	//	{
	//		if(m_TargetZones[i].m_InUse)
	//		{
	//			fTotalWeight += (float)m_TargetZones[i].m_TargetCount;
	//			iNumZones++;
	//		}			
	//	}

	//	float fRand = Mathf::IntervalRandom(0.f, fTotalWeight);
	//	for(int i = 0; i < MaxTargetZones; ++i)
	//	{
	//		if(m_TargetZones[i].m_InUse)
	//		{
	//			fRand -= (float)m_TargetZones[i].m_TargetCount;
	//			if(fRand < 0.f)
	//			{
	//				m_AimPosition = m_TargetZones[i].m_Position;
	//				m_ValidAim = true;
	//				return;
	//			}
	//		}
	//	}
	//	m_ValidAim = false;
	//}

	//void TrackTargetZone::RenderDebug()
	//{
	//	for(int i = 0; i < MaxTargetZones; ++i)
	//	{
	//		if(m_TargetZones[i].m_InUse)
	//		{
	//			Utils::DrawRadius(
	//				m_TargetZones[i].m_Position,
	//				m_Radius, 
	//				COLOR::MAGENTA, 
	//				MIN_RENDER_TIME);

	//			Utils::PrintText(
	//				m_TargetZones[i].m_Position,
	//				COLOR::WHITE,
	//				1.f, 
	//				"%d",
	//				m_TargetZones[i].m_TargetCount);
	//		}
	//	}
	//}

	//void TrackTargetZone::Update(Client *_client)
	//{
	//	const MemoryRecord *pTargetRec = _client->GetTargetingSystem()->GetCurrentTargetRecord();
	//	if(pTargetRec != NULL && pTargetRec->GetEntity() != m_LastTarget)
	//	{
	//		for(int i = 0; i < MaxTargetZones; ++i)
	//		{
	//			// if new target, add it to the zone counter
	//			bool bFound = false;
	//			TargetZone *pFreeZone = 0;
	//			for(int z = 0; z < MaxTargetZones; ++z)
	//			{
	//				if(m_TargetZones[z].m_InUse)
	//				{
	//					const float fSqDistance = SquaredLength(m_TargetZones[z].m_Position,pTargetRec->GetLastSensedPosition());

	//					if(fSqDistance < Mathf::Sqr(m_Radius))
	//					{
	//						m_TargetZones[z].m_TargetCount++;
	//						bFound = true;
	//					}
	//				}
	//				else
	//				{
	//					if(!pFreeZone)
	//						pFreeZone = &m_TargetZones[z];
	//				}
	//			}

	//			if(!bFound && pFreeZone)
	//			{
	//				pFreeZone->m_InUse = true;
	//				pFreeZone->m_Position = pTargetRec->GetLastSensedPosition();
	//				pFreeZone->m_TargetCount = 1;
	//			}
	//		}

	//		m_LastTarget = pTargetRec->GetEntity();
	//	}

	//	if(m_LastTarget.IsValid() && !InterfaceFuncs::IsAlive(m_LastTarget))
	//		m_LastTarget.Reset();
	//}

	//////////////////////////////////////////////////////////////////////////
#if ENABLE_FLOOD_FILLER
	void FloodFiller::Connection::Reset()
	{
		Destination = 0;
		Jump = false;
	}

	void FloodFiller::Node::Init(obint16 _X, obint16 _Y, float _Height, bool _Open /* = false */)
	{
		MinOffset.X = MaxOffset.X = _X;
		MinOffset.Y = MaxOffset.Y = _Y;

		Height = _Height;
		Open = _Open;

		OpenNess = 0;

		for(int i = 0; i < DIR_NUM; ++i)
			Connections[i].Reset();

		NearObject = false;
		NearEdge = false;
		Sectorized = false;
		SectorId = 0;
	}

	//////////////////////////////////////////////////////////////////////////

	const float CHARACTER_HEIGHT = 64.f;
	const float CHARACTER_STEPHEIGHT = 18.f;
	const float CHARACTER_JUMPHEIGHT = 60.f;
	const float MIN_COVER_HEIGHT = 40.f;
	const float MAX_COVER_HEIGHT = 60.f;

	FloodFiller::FloodFiller() 
		: StateChild("FloodFiller")
	{
		Reset();
		State = FillDone;
	}

	Vector3f FloodFiller::_GetNodePosition(const Node &_Node)
	{
		Vector3f vNodePos = Start;
		
		const float nX = (float)(_Node.MinOffset.X + _Node.MaxOffset.X) * 0.5f;
		const float nY = (float)(_Node.MinOffset.Y + _Node.MaxOffset.Y) * 0.5f;

		vNodePos.x += ((Radius*2.f) * nX);
		vNodePos.y += ((Radius*2.f) * nY);
		vNodePos.z = _Node.Height;

		return vNodePos;
	}

	FloodFiller::Node *FloodFiller::_NodeExists(obint16 _X, obint16 _Y, float _Height)
	{
		for(int i = 0; i < FreeNode; ++i)
		{
			if(Nodes[i].MinOffset.X >= _X && Nodes[i].MinOffset.Y >= _Y && 
				Nodes[i].MaxOffset.X <= _X && Nodes[i].MaxOffset.Y <= _Y)
			{
				if(Mathf::FAbs(_Height-Nodes[i].Height) < CHARACTER_HEIGHT)
					return &Nodes[i];
			}
		}
		return NULL;
	}

	FloodFiller::Node *FloodFiller::_NextOpenNode()
	{
		for(int i = 0; i < FreeNode; ++i)
		{
			if(Nodes[i].Open)
			{
				return &Nodes[i];					
			}
		}
		return NULL;
	}

	bool FloodFiller::_TestNode(const Node *_Node)
	{
		Vector3f vPos = _GetNodePosition(*_Node);

		obTraceResult tr;
		EngineFuncs::TraceLine(tr, 
			vPos,
			vPos,
			&FloodBlock,
			TR_MASK_FLOODFILL,
			-1, 
			False);

		return (tr.m_Fraction == 1.0f);
	}

	bool FloodFiller::_DropToGround(Node *_Node)
	{
		Vector3f vPos = _GetNodePosition(*_Node);

		const float fDropDistance = 512.f;

		AABB bounds = FloodBlock;
		bounds.m_Maxs[2] = CHARACTER_HEIGHT - CHARACTER_JUMPHEIGHT;

		static float START_OFFSET = CHARACTER_JUMPHEIGHT + 10.f;
		static float END_OFFSET = -fDropDistance;

		obTraceResult tr;
		EngineFuncs::TraceLine(tr, 
			vPos.AddZ(START_OFFSET),
			vPos.AddZ(END_OFFSET),
			&bounds,
			TR_MASK_FLOODFILL,
			-1, 
			False);

		bool bGood = true;

		// adjust node height
		if(tr.m_Fraction < 1.f)
			_Node->Height = tr.m_Endpos[2] + CHARACTER_STEPHEIGHT;

		if(tr.m_StartSolid)
			bGood = false;

		return bGood;
	}

	void FloodFiller::_MakeConnection(Node *_NodeA, Node *_NodeB, Direction _Dir)
	{
		{
			_NodeA->Connections[_Dir].Destination = _NodeB;
			_NodeA->Connections[_Dir].Jump = (_NodeB->Height - _NodeA->Height) >= CHARACTER_STEPHEIGHT;
			
			if((_NodeB->Height - _NodeA->Height) >= MIN_COVER_HEIGHT &&
				(_NodeB->Height - _NodeA->Height) <= MAX_COVER_HEIGHT)
			{
				_NodeA->Connections[_Dir].Cover |= (1<<_Dir);
			}
		}

		{
			//// connect the opposite direction
			//Direction Opposite[DIR_NUM] = 
			//{
			//	DIR_SOUTH,
			//	DIR_WEST,
			//	DIR_NORTH,
			//	DIR_EAST
			//};

			//_Dir = Opposite[_Dir];
			//_NodeB->Connections[_Dir].Destination = _NodeB;
			//_NodeB->Connections[_Dir].Jump = (_NodeA->Height - _NodeB->Height) >= CHARACTER_STEPHEIGHT;
			//_NodeB->Connections[_Dir].Cover = (_NodeA->Height - _NodeB->Height) >= MIN_COVER_HEIGHT &&
			//	(_NodeA->Height - _NodeB->Height) <= MAX_COVER_HEIGHT;
		}
	}

	void FloodFiller::_FillOpenNess(bool _ResetAll)
	{
		//////////////////////////////////////////////////////////////////////////
		for(int i = 0; i < FreeNode; ++i)
		{
			if(_ResetAll)
				Nodes[i].OpenNess = 0;

			if(!Nodes[i].Sectorized)
			{
				if(Nodes[i].NearEdge || Nodes[i].NearObject)
					Nodes[i].OpenNess = 1;

				// bordering another sector also resets
				for(int d = 0; d < DIR_NUM; ++d)
				{
					if(Nodes[i].Connections[d].Destination)
					{
						if(Nodes[i].Connections[d].Destination->Sectorized)
							Nodes[i].OpenNess = 1;
					}
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////

		obuint8 iCurrentDistance = 1;
		bool bKeepGoing = true;
		while(bKeepGoing)
		{
			bKeepGoing = false;

			for(int i = 0; i < FreeNode; ++i)
			{
				if(Nodes[i].OpenNess == iCurrentDistance)
				{
					for(int d = 0; d < DIR_NUM; ++d)
					{
						if(Nodes[i].Connections[d].Destination)
						{
							if(!Nodes[i].Connections[d].Destination->OpenNess)
							{
								bKeepGoing = true;
								Nodes[i].Connections[d].Destination->OpenNess = Nodes[i].OpenNess+1;
							}
						}
					}
				}
			}
			++iCurrentDistance;
		}
	}

	bool FloodFiller::_CanMergeWith(Node *_Node, Node *_WithNode)
	{
		if(!_WithNode)
			return false;

		if(Mathf::FAbs(_WithNode->Height - _Node->Height) >= CHARACTER_STEPHEIGHT)
			return false;

		if(_WithNode->SectorId != 0)
			return false;

		return true;
	}

	void FloodFiller::_MergeSectors()
	{
		_FillOpenNess(true);

		obuint16 NextSectorId = 0;

		for(;;)
		{
			NextSectorId++;
			//////////////////////////////////////////////////////////////////////////
			Node *pLargestNode = 0;
			obuint8 iLargestOpenness = 0;
			for(int i = 0; i < FreeNode; ++i)
			{
				if(!Nodes[i].Sectorized && Nodes[i].OpenNess > iLargestOpenness)
				{
					pLargestNode = &Nodes[i];
					iLargestOpenness = Nodes[i].OpenNess;
				}
			}
			if(!pLargestNode)
				break;
			//////////////////////////////////////////////////////////////////////////
			if(pLargestNode)
			{
				/*Utils::PrintText(
					_GetNodePosition(*pLargestNode) + Vector3f(0,0,64),
					30.f,
					"%d",NextSectorId);*/

				pLargestNode->Sectorized = true;
				pLargestNode->SectorId = NextSectorId;

				Node s;
				s.Init(pLargestNode->MinOffset.X,pLargestNode->MinOffset.Y,pLargestNode->Height,false);

				int ExpansionMask = (1<<DIR_NORTH)|(1<<DIR_EAST)|(1<<DIR_SOUTH)|(1<<DIR_WEST);

				int NumMergeNodes = 0;
				Node *MergeNodes[2048] = {};

				MergeNodes[NumMergeNodes++] = pLargestNode;

				while(ExpansionMask)
				{
					for(int d = DIR_NORTH; d < DIR_NUM; ++d)
					{
						int NumMergeEdge = 0;
						Node *MergeEdge[2048] = {};

						if(ExpansionMask & (1<<d))
						{
							switch(d)
							{
							case DIR_NORTH:
							case DIR_SOUTH:
								{
									//for(int c = 0)

									obint16 y = 0;
									if(d==DIR_NORTH)
										y = s.MaxOffset.Y + 1;
									else
										y = s.MinOffset.Y - 1;
									for(obint16 x = s.MinOffset.X; x <= s.MaxOffset.X; ++x)
									{
										Node *pNext = _NodeExists(x,y,s.Height);
										if(!_CanMergeWith(pLargestNode,pNext))
										{
											/*Node tn;
											tn.MinOffset.X = tn.MaxOffset.X = x;
											tn.MinOffset.Y = tn.MaxOffset.Y = y;
											tn.Height = s.Height;
											Vector3f vMissing = _GetNodePosition(tn);
											Utils::DrawLine(
												vMissing,
												vMissing + Vector3f(0,0,64),
												COLOR::ORANGE,
												20.f);*/

											ExpansionMask &= ~(1<<d);
											break;
										}

										MergeEdge[NumMergeEdge++] = pNext;
									}
									break;
								}
							case DIR_EAST:
							case DIR_WEST:
								{
									obint16 x = 0;
									if(d==DIR_EAST)
										x = s.MaxOffset.X + 1;
									else
										x = s.MinOffset.X - 1;
									for(obint16 y = s.MinOffset.Y; y <= s.MaxOffset.Y; ++y)
									{
										Node *pNext = _NodeExists(x,y,s.Height);
										if(!_CanMergeWith(pLargestNode,pNext))
										{
											/*Node tn;
											tn.MinOffset.X = tn.MaxOffset.X = x;
											tn.MinOffset.Y = tn.MaxOffset.Y = y;
											tn.Height = s.Height;
											Vector3f vMissing = _GetNodePosition(tn);
											Utils::DrawLine(
												vMissing,
												vMissing + Vector3f(0,0,64),
												COLOR::ORANGE,
												20.f);*/


											ExpansionMask &= ~(1<<d);
											break;
										}

										MergeEdge[NumMergeEdge++] = pNext;
									}
									break;
								}
							}
							//////////////////////////////////////////////////////////////////////////
							if(ExpansionMask & (1<<d))
							{
								// adjust the bounds
								switch(d)
								{
								case DIR_NORTH:
									s.MaxOffset.Y++;
									break;
								case DIR_SOUTH:
									s.MinOffset.Y--;
									break;
								case DIR_EAST:
									s.MaxOffset.X++;
									break;
								case DIR_WEST:
									s.MinOffset.X--;
									break;
								}

								// add to the merge list
								for(int m = 0; m < NumMergeEdge; ++m)
								{
									MergeEdge[m]->Sectorized = true;
									MergeEdge[m]->SectorId = NextSectorId;
									MergeNodes[NumMergeNodes++] = MergeEdge[m];
								}
							}
						}
					}
				}

				_FillOpenNess(false);

				// done merging sector, debug render
				Node *EdgeNodes[DIR_NUM][2048] = {};
				int NumEdges[DIR_NUM] = {};

				for(int m = 0; m < NumMergeNodes; ++m)
				{
					for(int d = DIR_NORTH; d < DIR_NUM; ++d)
					{
						bool bHasDirectionConnection = false;
						if(MergeNodes[m]->Connections[d].Destination)
						{
							bHasDirectionConnection = true;
							if(MergeNodes[m]->SectorId != MergeNodes[m]->Connections[d].Destination->SectorId)
							{								
								EdgeNodes[d][NumEdges[d]++] = MergeNodes[m];
							}
						}
						if(!bHasDirectionConnection)
							EdgeNodes[d][NumEdges[d]++] = MergeNodes[m];
					}
				}

				static float RT = 10.f;
				if(RT>0.f)
				{
					for(int d = DIR_NORTH; d < DIR_NUM; ++d)
					{
						Vector3f vP1, vP2;
						float fFurthestPoints = 0.f;

						for(int i1 = 0; i1 < NumEdges[d]; ++i1)
						{
							for(int i2 = 0; i2 < NumEdges[d]; ++i2)
							{
								Vector3f p1 = _GetNodePosition(*EdgeNodes[d][i1]);
								Vector3f p2 = _GetNodePosition(*EdgeNodes[d][i2]);
								float fDist = Length(p1,p2);

								if(fFurthestPoints==0.f)
								{
									vP1 = p1;
									vP2 = p2;
								}

								if(fDist > fFurthestPoints)
								{
									fFurthestPoints = fDist;
									vP1 = p1;
									vP2 = p2;
								}
							}
						}
						if(fFurthestPoints>0.f)
						{
							Utils::DrawLine(
								vP1 + Vector3f(0,0,64),
								vP2 + Vector3f(0,0,64),
								COLOR::BLUE,RT);
						}
					}
				}
			}
		}
	}

	void FloodFiller::Reset()
	{
		for(int i = 0; i < NumSectors; ++i)
		{
			Nodes[i].Init();
		}
		FreeNode = 0;
	}

	void FloodFiller::NextFillState()
	{
		if(State < FillDone)
			State = (FillState)(State+1);
	}

	void FloodFiller::StartFloodFill(const Vector3f &_Start, float _Radius)
	{
		Start = _Start;
		Radius = _Radius;

		FloodBlock = AABB(Vector3f::ZERO);
		FloodBlock.ExpandAxis(0,Radius);
		FloodBlock.ExpandAxis(1,Radius);
		FloodBlock.m_Maxs[2] = CHARACTER_HEIGHT - CHARACTER_STEPHEIGHT;

		State = FillInit;
	}

	void FloodFiller::GetDebugString(StringStr &out)
	{

	}

	void FloodFiller::RenderDebug()
	{
		static int MS_DELAY = 1000;
		static int NEXT_UPDATE = 0;
		if(IGame::GetTime() < NEXT_UPDATE)
			return;

		Vector3f vAim;
		Utils::GetLocalAimPoint(vAim);

		float fNearestSector = 99999999.f;
		const Node *pNearestSector = 0;

		NEXT_UPDATE = IGame::GetTime() + MS_DELAY;

		const float RENDER_TIME = Utils::MillisecondsToSeconds(MS_DELAY);
		for(int i = 0; i < FreeNode; ++i)
		{
			//if(FloodNode[i].OffsetX && FloodNode[i].OffsetY)
			{
				const Node &node = Nodes[i];

				obColor nodeCol = COLOR::GREEN;
				if(node.NearEdge)
					nodeCol = COLOR::MAGENTA;
				if(node.NearObject)
					nodeCol = COLOR::RED;
				
				Vector3f vNodePos = _GetNodePosition(node);

				static bool RenderOpenness = false;
				if (RenderOpenness)
				{
					Utils::PrintTextArgs(
						vNodePos,
						COLOR::WHITE,
						RENDER_TIME,
						"%d",
						node.OpenNess);
				}
				

				float fSectorDist = Length(vNodePos,vAim);
				if(fSectorDist < fNearestSector)
				{
					fNearestSector = fSectorDist;
					pNearestSector = &node;
				}

				for(int d = 0; d < DIR_NUM; ++d)
				{
					if(node.Connections[d].Destination)
					{						
						Vector3f vNeighbor = _GetNodePosition(*node.Connections[d].Destination);
						Utils::DrawLine(vNodePos.AddZ(8), vNeighbor, nodeCol, RENDER_TIME);

						const float fDist = Length(vNodePos, vNeighbor);
						if(fDist > 128.f)
						{
							Utils::OutputDebug(kNormal,"bad connection: %f", fDist);
						}
					}

					static bool bDrawCover = false;
					if(bDrawCover)
					{
						if(node.Connections[d].Cover)
						{
							obint16 Offset[DIR_NUM][2] =
							{
								{ 0, 1 },
								{ 1, 0 },
								{ 0, -1 },
								{ -1, 0 },
							};

							Vector3f vNodePos2 = _GetNodePosition(node);
							for(int j = 0; j < DIR_NUM; ++j)
							{
								if(node.Connections[d].Cover & (1<<j))
								{
									Vector3f vNeighborPos = vNodePos2;
									vNeighborPos.x += (Radius*2.f) * Offset[j][0];
									vNeighborPos.y += (Radius*2.f) * Offset[j][1];
									Utils::DrawLine(
										vNodePos2.AddZ(32),
										vNeighborPos.AddZ(32),
										COLOR::BLUE,
										RENDER_TIME);
								}
							}
						}
					}
				}
			}
		}

		if(pNearestSector)
		{
			//int iNumConnections = 0;
			//for(int d = 0; d < DIR_NUM; ++d)
			//	if(pNearestSector->Connections[d].Destination)
			//		++iNumConnections;
			Vector3f vNodePos = _GetNodePosition(*pNearestSector);
			Utils::PrintTextArgs(
				vNodePos+Vector3f(0,0,32),
				COLOR::WHITE,
				RENDER_TIME,
				"%d, sid %d",
				pNearestSector-Nodes,
				pNearestSector->Sectorized?pNearestSector->SectorId:-1);
			Utils::DrawLine(vNodePos, vNodePos + Vector3f(0,0,32.f),COLOR::CYAN,RENDER_TIME);
		}
	}

	obReal FloodFiller::GetPriority()
	{
		//return State != FillDone ? 1.f : 0.f;
		return 1.f;
	}
	void FloodFiller::Enter()
	{
		DebugDraw(true);
	}
	void FloodFiller::Exit()
	{	
		//DebugDraw(false);
	}
	State::StateStatus FloodFiller::Update(float fDt)
	{
		//Timer tmr;
		if(Length(Start,GetClient()->GetPosition()) > 256.f)
			StartFloodFill(GetClient()->GetPosition());

		//////////////////////////////////////////////////////////////////////////
		if(State == FillInit)
		{
			Reset();

			// Add a starting flood node
			Nodes[FreeNode++].Init(0,0,Start.z,true);

			_DropToGround(&Nodes[0]);

			NextFillState();
		}
		//////////////////////////////////////////////////////////////////////////
		while(State == FillSearching)
		{
			Node *currentNode = _NextOpenNode();
			if(!currentNode)
			{
				NextFillState();
				return State_Busy;
			}

			currentNode->Open = false;

			// Expand in each direction
			for(int d = DIR_NORTH; d < DIR_NUM; ++d)
			{
				obint16 Offset[DIR_NUM][2] =
				{
					{ 0, 1 },
					{ 1, 0 },
					{ 0, -1 },
					{ -1, 0 },
				};

				obint16 iOffX = currentNode->MinOffset.X + Offset[d][0];
				obint16 iOffY = currentNode->MinOffset.Y + Offset[d][1];
				float fHeight = currentNode->Height;

				Node *pNeighbor = _NodeExists(iOffX, iOffY, fHeight);
				if(pNeighbor)
				{
					_MakeConnection(currentNode,pNeighbor,(Direction)d);
					continue;
				}

				if(FreeNode==NumSectors)
				{
					_MergeSectors();					

					// fail if that didn't free up anything.
					if(FreeNode==NumSectors)
					{
						NextFillState();
						return State_Busy;
					}
				}

				// next node
				pNeighbor = &Nodes[FreeNode];
				pNeighbor->Init(iOffX,iOffY,fHeight,false);

				static float FLOOD_RADIUS = 512.f;
				Vector3f vNodePos = _GetNodePosition(*pNeighbor);
				if(Length(vNodePos, Start) > FLOOD_RADIUS)
				{
					currentNode->NearEdge = true;
					continue;
				}

				if(!_DropToGround(pNeighbor))
				{
					currentNode->NearObject |= true;
					continue;
				}

				currentNode->NearEdge |= (currentNode->Height - pNeighbor->Height) > CHARACTER_JUMPHEIGHT;

				// make the connection.
				_MakeConnection(currentNode,pNeighbor,(Direction)d);

				pNeighbor->Open = true;

				++FreeNode;
			}

			if(currentNode->NearObject || currentNode->NearEdge)
				currentNode->OpenNess = 1;
		}
		//////////////////////////////////////////////////////////////////////////
		if(State==FillOpenNess)
		{
			NextFillState();
			//_FillOpenNess();
		}
		//////////////////////////////////////////////////////////////////////////
		if(State==FillSectorize)
		{
			_MergeSectors();
			NextFillState();
		}
		//////////////////////////////////////////////////////////////////////////
		if(State == FillDone)
		{

		}
		return State_Busy;
	}

	//////////////////////////////////////////////////////////////////////////
#endif
}
