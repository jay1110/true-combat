#ifndef TCE_DOOR_QUEUE_H
#define TCE_DOOR_QUEUE_H

// A process-local, bounded passage lease. Keys include entity serials, so a
// recycled player/mover slot cannot inherit an earlier occupant's turn.
// No engine services are hidden here: callers remove dead/disconnected users.
class TceDoorQueue
{
public:
    enum { Capacity = 64, HeartbeatMs = 2000, WaitMs = 60000, PassageMs = 45000 };
    struct Ticket
    {
        int bot, botId, door, model, joined, last, granted;
        bool active, owner;
        Ticket() : bot(0), botId(0), door(0), model(0), joined(0), last(0),
            granted(0), active(false), owner(false) {}
    };
    struct Result
    {
        int rank;
        bool owner;
        Result(int r = -1, bool o = false) : rank(r), owner(o) {}
    };

    TceDoorQueue() : m_LastTime(-1) {}
    const Ticket &Get(int index) const { return m_Tickets[index]; }
    void Release(int bot)
    {
        for(int i = 0; i < Capacity; ++i)
            if(m_Tickets[i].active && m_Tickets[i].bot == bot)
                m_Tickets[i] = Ticket();
    }
    Result Request(int now, int bot, int botId, int door, int model, bool permit)
    {
        if(now < 0) return Result();
        if(now < m_LastTime)
            for(int i = 0; i < Capacity; ++i) m_Tickets[i] = Ticket();
        m_LastTime = now;
        int slot = -1, available = -1;
        for(int i = 0; i < Capacity; ++i)
        {
            Ticket &ticket = m_Tickets[i];
            if(ticket.active && (now < ticket.last || now-ticket.last > HeartbeatMs))
                ticket = Ticket();
            if(ticket.active && ticket.bot == bot) slot = i;
            if(!ticket.active && available < 0) available = i;
        }
        if(slot >= 0 && (m_Tickets[slot].door != door || m_Tickets[slot].model != model))
        {
            m_Tickets[slot] = Ticket();
            available = slot;
            slot = -1;
        }
        if(slot < 0)
        {
            if(available < 0) return Result();
            slot = available;
            Ticket &ticket = m_Tickets[slot];
            ticket.active = true;
            ticket.bot = bot; ticket.botId = botId;
            ticket.door = door; ticket.model = model;
            ticket.joined = now;
        }
        Ticket &self = m_Tickets[slot];
        self.last = now;
        if(now-self.joined >= WaitMs || (self.owner && now-self.granted >= PassageMs))
        {
            self = Ticket();
            return Result();
        }
        int rank = 0;
        bool occupied = false;
        for(int i = 0; i < Capacity; ++i)
        {
            const Ticket &other = m_Tickets[i];
            if(!other.active || other.door != door || other.model != model || i == slot) continue;
            if(other.owner) occupied = true;
            if(other.owner || other.joined < self.joined ||
                (other.joined == self.joined && other.botId < self.botId)) ++rank;
        }
        // One update of admission delay makes same-frame joins independent of
        // bot update order. An existing passage owner is never preempted.
        if(!self.owner && !occupied && rank == 0 && permit && now > self.joined)
        {
            self.owner = true;
            self.granted = now;
        }
        return Result(rank, self.owner && permit);
    }
private:
    Ticket m_Tickets[Capacity];
    int m_LastTime;
};

#endif
