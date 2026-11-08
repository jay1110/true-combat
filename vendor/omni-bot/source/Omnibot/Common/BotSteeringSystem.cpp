#include "PrecompCommon.h"
#include "BotSteeringSystem.h"
#include "Client.h"
#include "IGame.h"
#include "IGameManager.h"

namespace AiState
{
	// A blocked bot can ask the next ally to make room before either has
	// physical velocity. Ordinary velocity-only avoidance cannot clear a packed
	// corridor. Requests retain live entity identities, expire quickly and may
	// travel through at most eight allies; no movement or collision is bypassed.
	struct TceTrafficPressure
	{
		GameEntity bot, human;
		Vector3f direction;
		int until, depth;
		TceTrafficPressure() : direction(Vector3f::ZERO), until(0), depth(0) {}
	};
	static TceTrafficPressure tceTrafficPressure[obPlayerInfo::MaxPlayers];
	static int tceTrafficTime = -1;
	static void TceTrafficClock(int now)
	{
		if(now < tceTrafficTime)
			for(int i = 0; i < obPlayerInfo::MaxPlayers; ++i) tceTrafficPressure[i] = TceTrafficPressure();
		tceTrafficTime = now;
	}

	SteeringSystem::SteeringSystem()
		: StateChild("SteeringSystem")
		, m_TargetVector	(Vector3f::ZERO)
		, m_TargetRadius	(32.f)
		, m_InputScale	(1.f)
		, m_MoveMode		(Run)
		, m_DistanceToTarget(0.f)
		, m_MoveVec			(Vector3f::ZERO)
		, m_NoAvoidTime		(0)
		, m_MoveType		(Normal)
		, m_bMoveEnabled	(true)
		, m_TargetVector3d	(false)
		, m_AvoidSide(1)
		, m_AvoidUntil(0)
		, m_AvoidDirection(Vector3f::ZERO)
	{
	}

	SteeringSystem::~SteeringSystem()
	{
	}

	void SteeringSystem::RenderDebug()
	{
		Utils::DrawLine(GetClient()->GetEyePosition(), GetTarget(), COLOR::GREEN, IGame::GetDeltaTimeSecs()*2.f);
	}

	bool SteeringSystem::InTargetRadius() const
	{
		return m_DistanceToTarget <= m_TargetRadius;
	}

	Vector3f SteeringSystem::GetMoveVector(MoveType _movetype)
	{
		return m_bMoveEnabled ? m_MoveVec : Vector3f::ZERO;
	}

	bool SteeringSystem::SetTarget(const Vector3f &_pos, float _radius, MoveMode _movemode, bool _in3d, float _inputScale)
	{
		m_TargetVector = _pos;
		m_TargetRadius = _radius;
		m_InputScale = ClampT(_inputScale, 0.f, 1.f);
		m_TargetVector3d = _in3d;
		m_MoveMode = _movemode;
		m_MoveType = Arrive;
		return true;
	}

	void SteeringSystem::SetNoAvoidTime(int _time)
	{
		m_NoAvoidTime = IGame::GetTime() + _time;
	}

	bool SteeringSystem::SafeAvoidStep(const Vector3f &_destination, GameEntity _human, int _depth) const
	{
		// CanWalkTo proves full-body support, slope, water and edge clearance.
		// A separate fresh body sweep also prevents stepping into any player.
		AABB hull;
		obTraceResult trace;
		if(!GetClient()->GetNavigationHull(hull, 0) || !GetClient()->CanWalkTo(_destination) ||
			!EngineFuncs::TraceLine(trace, GetClient()->GetPosition(), _destination, &hull,
				TR_MASK_SOLID | TR_MASK_PLAYERCLIP | TR_MASK_PLAYER | TR_MASK_NAVCAPSULE,
				GetClient()->GetGameID(), False)) return false;
		if(!trace.m_StartSolid && trace.m_Fraction == 1.f) return true;
		if(_human.IsValid() && _depth < 8 && !trace.m_StartSolid && trace.m_HitEntity.IsValid())
		{
			const int id = g_EngineFuncs->IDFromEntity(trace.m_HitEntity);
			obPlayerInfo players;
			g_EngineFuncs->GetPlayerInfo(players);
			if(id >= 0 && id < obPlayerInfo::MaxPlayers && id != GetClient()->GetGameID() &&
				players.m_Players[id].m_Controller == obPlayerInfo::Bot &&
				players.m_Players[id].m_Team == GetClient()->GetTeam() &&
				GetClient()->IsAllied(trace.m_HitEntity) && InterfaceFuncs::IsAlive(trace.m_HitEntity))
			{
				TceTrafficPressure &request = tceTrafficPressure[id];
				// Do not let a request circle back through the crowd and replace
				// a nearer source with a longer, contradictory relay chain.
				if(request.bot == trace.m_HitEntity &&
					(obint32)((obuint32)request.until-(obuint32)IGame::GetTime()) > 0 &&
					request.human == _human && request.depth < _depth+1) return false;
				request.bot = trace.m_HitEntity; request.human = _human;
				request.direction = (_destination-GetClient()->GetPosition()).Flatten();
				request.direction.Normalize();
				request.until = (obint32)((obuint32)IGame::GetTime()+1200u); request.depth = _depth+1;
			}
		}
		return false;
	}

	void SteeringSystem::AvoidAllies()
	{
		const int now = IGame::GetTime();
		TceTrafficClock(now);
		Client *client = GetClient();
		if (!client->UseValidatedWalkEntries() || m_TargetVector3d || !(m_TargetRadius >= 16.f) ||
			!(m_InputScale >= .999f) || m_NoAvoidTime > now || client->GetTeam() <= 0 ||
			client->HasEntityFlag(ENT_FLAG_DEAD) || client->HasEntityFlag(ENT_FLAG_DISABLED) ||
			!client->HasEntityFlag(ENT_FLAG_ONGROUND) || client->HasEntityFlag(ENT_FLAG_ONLADDER) ||
			client->HasEntityFlag(ENT_FLAG_ON_ICE) ||
			client->IsButtonDown(BOT_BUTTON_JUMP))
		{
			const int id = client->GetGameID();
			if(id >= 0 && id < obPlayerInfo::MaxPlayers) tceTrafficPressure[id] = TceTrafficPressure();
			m_AvoidEntity.Reset(); return;
		}
		Path::PathPoint point;
		const Path &path = client->GetFollowPath()->GetCurrentPath();
		const NavFlags exact = F_NAV_DOOR | F_NAV_JUMP | F_NAV_JUMPGAP | F_NAV_JUMPLOW |
			F_NAV_CLIMB | F_NAV_ELEVATOR | F_NAV_TELEPORT | F_NAV_PRONE | F_NAV_INWATER | F_NAV_UNDERWATER;
		for (int next = 0; next < 2; ++next)
		{
			if (!(next ? path.GetNextPt(point) : path.GetCurrentPt(point))) continue;
			if ((point.m_NavFlags & exact) || point.m_TceDoor.marked || point.m_OnPathThrough ||
				point.m_TceAscentRelease || point.m_TceAscentAlignment || point.m_TceWalkDrop ||
				point.m_TceStandJump || point.m_TceFreeClimb)
			{
				const int id = client->GetGameID();
				if(id >= 0 && id < obPlayerInfo::MaxPlayers) tceTrafficPressure[id] = TceTrafficPressure();
				m_AvoidEntity.Reset(); return;
			}
		}
		obPlayerInfo players;
		g_EngineFuncs->GetPlayerInfo(players);
		const Vector3f position = client->GetPosition();
		TceTrafficPressure pressure;
		const int self = client->GetGameID();
		if(self >= 0 && self < obPlayerInfo::MaxPlayers)
		{
			pressure = tceTrafficPressure[self];
			const int source = pressure.human.IsValid() ? g_EngineFuncs->IDFromEntity(pressure.human) : -1;
			Vector3f humanPosition;
			if((obint32)((obuint32)pressure.until-(obuint32)now) <= 0 || pressure.bot != client->GetGameEntity() ||
				source < 0 || source >= obPlayerInfo::MaxPlayers ||
				players.m_Players[source].m_Controller != obPlayerInfo::Human ||
				players.m_Players[source].m_Team != client->GetTeam() ||
				!InterfaceFuncs::IsAlive(pressure.human) ||
				!EngineFuncs::EntityPosition(pressure.human, humanPosition) ||
				(position-humanPosition).SquaredLength() > 512.f*512.f)
			{
				pressure = TceTrafficPressure();
				tceTrafficPressure[self] = pressure;
			}
		}
		Vector3f desired = m_MoveVec.Flatten();
		const bool moving = desired.Normalize() > .01f;
		GameEntity blocker;
		Vector3f relative;
		bool human = false;
		float bestDistance = 128.f * 128.f;
		for (int id = 0; id < obPlayerInfo::MaxPlayers; ++id)
		{
			if (id == client->GetGameID() || players.m_Players[id].m_Team != client->GetTeam()) continue;
			const GameEntity entity = g_EngineFuncs->EntityFromID(id);
			Vector3f other, velocity = Vector3f::ZERO;
			BitFlag64 flags;
			BitFlag32 category;
			if (!entity.IsValid() || !InterfaceFuncs::GetEntityCategory(entity, category) ||
				!category.CheckFlag(ENT_CAT_PLAYER) || !client->IsAllied(entity) ||
				!InterfaceFuncs::GetEntityFlags(entity, flags) || flags.CheckFlag(ENT_FLAG_DEAD) ||
				flags.CheckFlag(ENT_FLAG_DISABLED) || !InterfaceFuncs::IsAlive(entity) ||
				!EngineFuncs::EntityPosition(entity, other)) continue;
			Vector3f delta = other - position;
			if (!(Mathf::FAbs(delta.z) <= 48.f)) continue;
			delta.z = 0.f;
			const float distance = delta.Normalize();
			const bool isHuman = players.m_Players[id].m_Controller == obPlayerInfo::Human;
			if (!(distance > .01f && distance < (isHuman ? 112.f : 80.f))) continue;
			EngineFuncs::EntityVelocity(entity, velocity);
			const bool approaching = velocity.Dot(delta) < -16.f;
			const bool humanContact = isHuman && distance < 48.f;
			if (moving ? (desired.Dot(delta) < .15f && !(isHuman && approaching) && !humanContact) :
				(!approaching && !humanContact)) continue;
			if (moving)
			{
				const float ahead = desired.Dot(delta);
				if (distance * distance * (1.f - ahead * ahead) > 48.f * 48.f) continue;
			}
			// One moving bot gives way in a head-on encounter; humans always
			// have priority. A stationary ally never has to win an ID contest.
			if (!isHuman && moving && approaching && client->GetGameID() < id) continue;
			if (blocker.IsValid() && (human && !isHuman)) continue;
			if (blocker.IsValid() && human == isHuman && distance * distance >= bestDistance) continue;
			// Proximity is not contact: a wall or a separate parallel lane
			// must not make an otherwise clear walking route yield. Require
			// a fresh full-body first hit on this exact live player identity.
			AABB hull;
			obTraceResult contact;
			if (!client->GetNavigationHull(hull, 0)) { m_MoveVec = Vector3f::ZERO; return; }
			if (!EngineFuncs::TraceLine(contact, position, other, &hull,
				TR_MASK_SOLID | TR_MASK_PLAYERCLIP | TR_MASK_PLAYER | TR_MASK_NAVCAPSULE,
				client->GetGameID(), False) || contact.m_StartSolid ||
				contact.m_Fraction >= 1.f || contact.m_HitEntity != entity) continue;
			blocker = entity; human = isHuman; relative = delta;
			bestDistance = distance * distance;
		}
		const bool relayed = pressure.human.IsValid() && !human;
		if(relayed)
		{
			blocker = pressure.human; human = true;
			relative = -pressure.direction;
		}
		if (!blocker.IsValid()) return;
		const bool cached = blocker == m_AvoidEntity &&
			(obint32)((obuint32)m_AvoidUntil - (obuint32)now) > 0;
		const int preferred = cached ? m_AvoidSide : ((client->GetGameID() & 1) ? 1 : -1);
		const bool directContact = human && !relayed && bestDistance < 48.f*48.f;
		if(cached && directContact && m_AvoidDirection.SquaredLength() > .5f)
		{
			// A side sign alone is unstable when the desired/relative frame
			// rotates at contact. Retain the chosen WORLD direction briefly,
			// with the same fresh full-body proof before every ordinary input.
			for(int attempt = 0; attempt < 2; ++attempt)
			{
				if(!SafeAvoidStep(position+m_AvoidDirection*(attempt ? 16.f : 32.f), blocker, 0)) continue;
				m_MoveVec = m_AvoidDirection*.65f;
				return;
			}
		}
		const Vector3f forward = relayed ? pressure.direction : moving ? desired : relative;
		const Vector3f side(-forward.y, forward.x, 0.f);
		// At actual human contact, the usual forward bias points into the
		// touching body and rejects both otherwise clear lateral sweeps.
		const float forwardBias = directContact ? 0.f : .2f;
		for (int attempt = 0; attempt < 6; ++attempt)
		{
			const int candidate = attempt % 3;
			const int sign = candidate == 0 ? preferred : -preferred;
			Vector3f direction = candidate == 2 ? -relative : side * (float)sign + forward * forwardBias;
			direction.Normalize();
			if (!SafeAvoidStep(position + direction * (attempt < 3 ? 32.f : 16.f),
				human ? blocker : GameEntity(), relayed ? pressure.depth : 0)) continue;
			m_MoveVec = direction * .65f;
			m_AvoidEntity = blocker; m_AvoidSide = sign;
			m_AvoidDirection = directContact ? direction : Vector3f::ZERO;
			// A newly chosen contact escape needs its own complete lease, even
			// when an older cached retreat just became blocked by the wall.
			if (!cached || directContact) m_AvoidUntil = (obint32)((obuint32)now + 750u);
			return;
		}
		// No supported full-body route: yield with ordinary zero input.
		// Never force a jump, alter physics or manufacture an open nav link.
		m_MoveVec = Vector3f::ZERO;
	}

	void SteeringSystem::UpdateSteering()
	{
		Prof(UpdateSteering);

		if(!m_bMoveEnabled)
		{
			m_MoveVec = Vector3f::ZERO;
			return;
		}

		// Calculate the vector to the current target.
		/*m_vMoveVec = (m_vTargetVector - m_Client->GetPosition());
		m_vMoveVec.Normalize();*/
		//obstacleManager.ModifyForObstacles( GetClient(), m_TargetVector );

		m_DistanceToTarget = 0.f;
		switch(m_MoveType)
		{
		case Normal:
			m_MoveVec = m_TargetVector - GetClient()->GetPosition();
			if(!m_TargetVector3d)
				m_MoveVec = m_MoveVec.Flatten();
			m_DistanceToTarget = m_MoveVec.Normalize();
			break;
		case Arrive:
			m_MoveVec = m_TargetVector - GetClient()->GetPosition();
			if(!m_TargetVector3d)
				m_MoveVec = m_MoveVec.Flatten();
			m_DistanceToTarget = m_MoveVec.Normalize();
			//m_vMoveVec *= _Arrive(m_vTargetVector, normal);
			break;
		}

		//////////////////////////////////////////////////////////////////////////
		if(m_DistanceToTarget <= m_TargetRadius)
		{
			m_MoveVec = Vector3f::ZERO;
			AvoidAllies();
			return;
		}
		// Optional input taper for a precise, physically stationary action
		// launch. Ordinary targets keep unit input; no velocity is modified.
		m_MoveVec *= m_InputScale;
		AvoidAllies();

		//////////////////////////////////////////////////////////////////////////

		if(GetClient()->IsDebugEnabled(BOT_DEBUG_MOVEVEC))
		{
			Vector3f vPos = GetClient()->GetPosition();
			Utils::DrawLine(vPos, vPos+m_MoveVec * 64.f, COLOR::GREEN, 0.1f);
		}

		if(GetClient()->HasEntityFlag(ENT_FLAG_ON_ICE))
		{
			GetClient()->PressButton(BOT_BUTTON_WALK);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	void SteeringSystem::Enter()
	{
		m_TargetVector = GetClient()->GetPosition();
		m_AvoidEntity.Reset(); m_AvoidUntil = 0;
		m_AvoidDirection = Vector3f::ZERO;
		GetClient()->SetMovementVector(Vector3f::ZERO);
		GetClient()->GetFollowPath()->Stop();
	}

	State::StateStatus SteeringSystem::Update(float fDt)
	{
		Prof(SteeringSystem);
		UpdateSteering();
		GetClient()->SetMovementVector(GetMoveVector());
		switch(m_MoveMode)
		{
		case Walk:
			GetClient()->PressButton(BOT_BUTTON_WALK);
			break;
		case Run:
		default:
			/*GetClient()->ReleaseButton(BOT_BUTTON_WALK);*/
			break;
		}
		return State_Busy;
	}
}
