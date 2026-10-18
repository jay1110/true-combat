#ifndef __PATH_H__
#define __PATH_H__

// class: Path
//		This class represents a traversable path through the environment.
class Path
{
public:
	// This recipe describes one directed, natively checked doorway transition.
	// Entity slots are never sufficient authorization: the live brush identities
	// and the actual activation eye trace are checked again before pressing USE.
	struct TceDoorRecipe
	{
		int entity, useEntity, target, mode, team, role, model, useModel, useNav, entry;
		bool marked;
		Vector3f aim;
		TceDoorRecipe() : entity(-1), useEntity(-1), target(0), mode(0), team(0), role(0),
			model(0), useModel(0), useNav(0), entry(0), marked(false), aim(Vector3f::ZERO) {}
		bool Valid() const { return entity >= 0 && useEntity >= 0 &&
			(role == 3 ? target >= 0 : target > 0) &&
			(mode == 1 || mode == 2 || mode == 3) && team > 0 && team <= 3 &&
			(role == 1 || role == 2 || role == 3) && model > 0 && useModel > 0 && useNav > 0 && entry > 0 &&
			(mode != 1 || (entity == useEntity && useNav == entry)) &&
            (mode != 3 || (entity == useEntity && model == useModel && useNav != entry)); }
        bool SameDoor(const TceDoorRecipe &other) const { return
            entity == other.entity && useEntity == other.useEntity && mode == other.mode &&
            team == other.team && model == other.model && useModel == other.useModel &&
            useNav == other.useNav && entry == other.entry &&
            aim.x == other.aim.x && aim.y == other.aim.y && aim.z == other.aim.z; }
	};
	struct PathPoint
	{
		Vector3f			m_Pt;
		float				m_Radius;
		NavFlags			m_NavFlags;

		int					m_NavId;
		obuint32			m_OnPathThrough;
		obuint32			m_OnPathThroughParam;
		bool                m_TceAscentRelease;
		bool                m_TceAscentAlignment;
		bool                m_TceWalkDrop;
		bool                m_TceStandJump;
		bool                m_TceFreeClimb;
		bool                m_IsPlayerOrigin;
		bool                m_TceDestinationVolume;
		TceDoorRecipe       m_TceDoor;

		PathPoint &Position(const Vector3f &aPos) { m_Pt = aPos; return *this;  }
		PathPoint &Radius(float aRadius) { m_Radius = aRadius; return *this;  }
		PathPoint &Flags(NavFlags aNavFlags) { m_NavFlags = aNavFlags; return *this;  }
		PathPoint &NavId(int aNavId) { m_NavId = aNavId; return *this; }
		PathPoint &OnPathThrough(obuint32 aPathThrough) { m_OnPathThrough = aPathThrough; return *this;  }
		PathPoint &OnPathThroughParam(obuint32 aPathThroughParam) { m_OnPathThroughParam = aPathThroughParam; return *this;  }
		PathPoint &TceAscentRelease(bool enabled) { m_TceAscentRelease = enabled; return *this; }
		PathPoint &TceAscentAlignment(bool enabled) { m_TceAscentAlignment = enabled; return *this; }
		PathPoint &TceWalkDrop(bool enabled) { m_TceWalkDrop = enabled; return *this; }
		PathPoint &TceStandJump(bool enabled) { m_TceStandJump = enabled; return *this; }
		PathPoint &TceFreeClimb(bool enabled) { m_TceFreeClimb = enabled; return *this; }
		PathPoint &TceDoor(const TceDoorRecipe &recipe) { m_TceDoor = recipe; return *this; }
		PathPoint &PlayerOrigin(bool enabled = true) { m_IsPlayerOrigin = enabled; return *this; }
		PathPoint &TceDestinationVolume(bool enabled = true) { m_TceDestinationVolume = enabled; return *this; }

		PathPoint()
			: m_Pt(Vector3f::ZERO)
			, m_Radius(0.f)
			, m_NavFlags(0)
			, m_NavId(0)
			, m_OnPathThrough(0)
			, m_OnPathThroughParam(0)
			, m_TceAscentRelease(false)
			, m_TceAscentAlignment(false)
			, m_TceWalkDrop(false)
			, m_TceStandJump(false)
			, m_TceFreeClimb(false)
			, m_IsPlayerOrigin(false)
			, m_TceDestinationVolume(false)
		{
		}
	};

	struct PathLink
	{
		float m_Distance;
		PathLink() : m_Distance(0.f) {}
	};

	void Clear();

	Path::PathPoint &AddPt(const Vector3f &_pt, float _radius);
	bool NextPt();

	void RemoveLastPt();

	Vector3f FindNearestPtOnPath(const Vector3f &_position, Vector3f *_outLookAhead = NULL, float _lookAheadDistance = 0.f);

	bool GetCurrentPt(PathPoint &_pt) const;
	bool GetNextPt(PathPoint &_pt) const;
	bool GetPreviousPt(PathPoint &_pt) const;
	bool GetFirstPt(PathPoint &_pt) const;
	bool GetLastPt(PathPoint &_pt) const;
	bool GetPt(int _index, PathPoint &_pt) const;
	const PathPoint *GetPt(int _index) const { return &m_Pts[_index]; }

	inline int GetNumPts() const { return m_NumPts; }
	inline int GetCurrentPtIndex() const { return m_CurrentPt; }
	inline bool IsEndOfPath() const { return m_CurrentPt >= m_NumPts-1; }

	float GetTotalLength() const;

	void DebugRender(obColor _color = COLOR::GREEN, float _duration = 5.f);

	Path();
private:
	static const int MAX_PATH_PTS = 512;
	PathPoint	m_Pts[MAX_PATH_PTS];
	PathLink	m_Links[MAX_PATH_PTS]; // store total distance at the end [MAX_PATH_PTS-1]

	// state info
	int		m_CurrentPt;
	int		m_NumPts;
};

#endif
