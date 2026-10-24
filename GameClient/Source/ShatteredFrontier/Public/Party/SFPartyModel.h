#pragma once

#include "CoreMinimal.h"

// Session party rules. IDs are assigned from server-owned controllers; no UObject or client identity enters this model.
class FSFPartyModel
{
public:
    static constexpr int32 MaxMembers = 5; // Dungeon Groups Part 4: solo_party.

    struct FParty
    {
        int32 Id = 0;
        TArray<int32> Members; // join order, used for automatic leader handoff.
        int32 Leader = 0;
        int32 Assistant = 0;
    };
    struct FInvite
    {
        int32 From = 0;
        int32 To = 0;
        int32 SourceParty = 0; // zero when the inviter was solo.
        double ExpiresAt = 0;
    };

    const FParty* FindParty(int32 Member) const
    {
        return Parties.FindByPredicate([Member](const FParty& P) { return P.Members.Contains(Member); });
    }
    const TArray<FInvite>& GetInvites() const { return Invites; }
    // Rebuild only connected members from a durable roster; offline IDs never enter the session model.
    int32 RestoreMember(int32 Member, int32 ExistingPartyId = 0)
    {
        if (!Member || FindParty(Member)) { return 0; }
        FParty* Party = Parties.FindByPredicate([=](const FParty& P) { return P.Id == ExistingPartyId; });
        if (!Party)
        {
            FParty NewParty; NewParty.Id = NextPartyId++; NewParty.Leader = Member; NewParty.Members.Add(Member);
            Parties.Add(MoveTemp(NewParty)); return Parties.Last().Id;
        }
        if (Party->Members.Num() >= MaxMembers) { return 0; }
        Party->Members.Add(Member); return Party->Id;
    }
    void RestoreRoles(int32 PartyId, int32 Leader, int32 Assistant)
    {
        if (FParty* Party = Parties.FindByPredicate([=](const FParty& P) { return P.Id == PartyId; }))
        {
            Party->Leader = Party->Members.Contains(Leader) ? Leader : Party->Members[0];
            Party->Assistant = Party->Members.Contains(Assistant) && Assistant != Party->Leader ? Assistant : 0;
        }
    }

    bool Invite(int32 From, int32 To, double Now, double Ttl)
    {
        if (!From || !To || From == To || FindParty(To) || Ttl <= 0) { return false; }
        const FParty* Party = FindParty(From);
        if (Party && (Party->Leader != From || Party->Members.Num() >= MaxMembers)) { return false; }
        Invites.RemoveAll([=](const FInvite& I) { return I.From == From && I.To == To; });
        Invites.Add({From, To, Party ? Party->Id : 0, Now + Ttl});
        return true;
    }
    bool Accept(int32 From, int32 To, double Now)
    {
        const int32 Index = Invites.IndexOfByPredicate([=](const FInvite& I) { return I.From == From && I.To == To; });
        if (Index == INDEX_NONE) { return false; }
        const FInvite InviteValue = Invites[Index];
        Invites.RemoveAt(Index);
        if (Now >= InviteValue.ExpiresAt || FindParty(To)) { return false; }
        FParty* Source = Parties.FindByPredicate([&](const FParty& P) { return P.Id == InviteValue.SourceParty; });
        if (InviteValue.SourceParty)
        {
            if (!Source || Source->Leader != From || Source->Members.Num() >= MaxMembers) { return false; }
        }
        else if (FindParty(From)) { return false; }
        if (!Source)
        {
            FParty NewParty;
            NewParty.Id = NextPartyId++;
            NewParty.Leader = From;
            NewParty.Members.Add(From);
            Parties.Add(MoveTemp(NewParty));
            Source = &Parties.Last();
        }
        Source->Members.Add(To);
        Invites.RemoveAll([=](const FInvite& I) { return I.To == To; });
        return true;
    }
    bool Decline(int32 From, int32 To)
    {
        return Invites.RemoveAll([=](const FInvite& I) { return I.From == From && I.To == To; }) > 0;
    }
    bool Leave(int32 Member)
    {
        FParty* Party = Parties.FindByPredicate([=](const FParty& P) { return P.Members.Contains(Member); });
        if (!Party) { return false; }
        Party->Members.Remove(Member);
        if (Party->Assistant == Member) { Party->Assistant = 0; }
        if (Party->Leader == Member)
        { Party->Leader = Party->Assistant ? Party->Assistant : (Party->Members.IsEmpty() ? 0 : Party->Members[0]); Party->Assistant = 0; }
        if (Party->Members.IsEmpty())
        {
            const int32 EmptyId = Party->Id;
            Parties.RemoveAll([=](const FParty& P) { return P.Id == EmptyId; });
        }
        Invites.RemoveAll([=](const FInvite& I) { return I.From == Member || I.To == Member; });
        return true;
    }
    bool Kick(int32 Leader, int32 Member)
    {
        const FParty* Party = FindParty(Leader);
        return Party && Party->Leader == Leader && Leader != Member && Party->Members.Contains(Member) && Leave(Member);
    }
    bool Promote(int32 Leader, int32 Member)
    {
        FParty* Party = Parties.FindByPredicate([=](const FParty& P) { return P.Leader == Leader; });
        if (!Party || Leader == Member || !Party->Members.Contains(Member)) { return false; }
        Party->Leader = Member;
        if (Party->Assistant == Member) { Party->Assistant = 0; }
        return true;
    }
    bool AssignAssistant(int32 Leader, int32 Member)
    {
        FParty* Party = Parties.FindByPredicate([=](const FParty& P) { return P.Leader == Leader; });
        if (!Party || Leader == Member || !Party->Members.Contains(Member)) { return false; }
        Party->Assistant = Member;
        return true;
    }
    void RemoveConnection(int32 Member)
    {
        Leave(Member);
        Invites.RemoveAll([=](const FInvite& I) { return I.From == Member || I.To == Member; });
    }
    void Expire(double Now)
    { Invites.RemoveAll([=](const FInvite& I) { return Now >= I.ExpiresAt; }); }

private:
    int32 NextPartyId = 1;
    TArray<FParty> Parties;
    TArray<FInvite> Invites;
};
