using Npgsql;

namespace ShatteredFrontier.Chat.Postgres;

public sealed record StoredMail(string MailId, string SenderAccountId, string SenderCharacterId,
    byte[] Subject, byte[] Body, string State, DateTimeOffset SentAt, DateTimeOffset? ReadAt);

public sealed record MailControls(int MailboxCapacity = 100, int HourlyLimit = 20,
    int MinIntervalSeconds = 30, int ExpiryDays = 30)
{
    public void Validate()
    {
        if (MailboxCapacity is < 1 or > 100 || HourlyLimit is < 1 or > 100 ||
            MinIntervalSeconds is < 1 or > 300 || ExpiryDays is < 1 or > 90)
            throw new ArgumentOutOfRangeException(nameof(MailControls));
    }
}

public sealed class PostgresMailStore
{
    private readonly string connectionString;
    private readonly MailControls controls;
    public PostgresMailStore(string connectionString, MailControls? controls = null)
    { this.connectionString = connectionString; this.controls = controls ?? new(); this.controls.Validate(); }
    private NpgsqlConnection Open() { var c = new NpgsqlConnection(connectionString); c.Open(); return c; }

    public string? Prior(string senderCharacter, string idempotencyKey, byte[] requestHash)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT mail_id,request_hash FROM chat.mail_messages WHERE sender_character_id=$1 AND sender_idempotency_key=$2", c);
        cmd.Parameters.AddWithValue(senderCharacter); cmd.Parameters.AddWithValue(idempotencyKey);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return null;
        if (!System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(r.GetFieldValue<byte[]>(1), requestHash))
            throw new InvalidOperationException("MAIL_IDEMPOTENCY_CONFLICT");
        return r.GetString(0);
    }

    public string Send(string mailId, string senderAccount, string senderCharacter, string recipientAccount, string recipientCharacter,
        string realm, byte[] subjectCt, byte[] bodyCt, string idempotencyKey, byte[] requestHash, DateTimeOffset now)
    {
        using var c = Open(); using var tx = c.BeginTransaction();
        using (var gate = new NpgsqlCommand("SELECT pg_advisory_xact_lock(hashtext($1), 726463)", c, tx))
        { gate.Parameters.AddWithValue(recipientCharacter); gate.ExecuteNonQuery(); }
        using (var gate = new NpgsqlCommand("SELECT pg_advisory_xact_lock(hashtext($1), 726464)", c, tx))
        { gate.Parameters.AddWithValue(senderCharacter); gate.ExecuteNonQuery(); }
        using (var prior = new NpgsqlCommand("SELECT mail_id,request_hash FROM chat.mail_messages WHERE sender_character_id=$1 AND sender_idempotency_key=$2", c, tx))
        {
            prior.Parameters.AddWithValue(senderCharacter); prior.Parameters.AddWithValue(idempotencyKey);
            using var r = prior.ExecuteReader();
            if (r.Read())
            {
                if (!System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(r.GetFieldValue<byte[]>(1), requestHash)) throw new InvalidOperationException("MAIL_IDEMPOTENCY_CONFLICT");
                return r.GetString(0);
            }
        }
        using (var capacity = new NpgsqlCommand("SELECT count(*) FROM chat.mail_messages WHERE recipient_character_id=$1 AND state IN ('delivered','read') AND expires_at>now()", c, tx))
        { capacity.Parameters.AddWithValue(recipientCharacter); if ((long)capacity.ExecuteScalar()! >= controls.MailboxCapacity) throw new InvalidOperationException("MAILBOX_FULL"); }
        using (var rate = new NpgsqlCommand("SELECT count(*),max(sent_at) FROM chat.mail_messages WHERE sender_character_id=$1 AND sent_at>$2", c, tx))
        {
            rate.Parameters.AddWithValue(senderCharacter); rate.Parameters.AddWithValue(now.AddHours(-1));
            using var r = rate.ExecuteReader(); r.Read();
            if (r.GetInt64(0) >= controls.HourlyLimit || (!r.IsDBNull(1) && now - r.GetFieldValue<DateTimeOffset>(1) < TimeSpan.FromSeconds(controls.MinIntervalSeconds)))
                throw new InvalidOperationException("MAIL_RATE_LIMITED");
        }
        var id = mailId;
        using var cmd = new NpgsqlCommand("INSERT INTO chat.mail_messages(mail_id,realm_id,sender_account_id,sender_character_id,recipient_account_id,recipient_character_id,subject_ct,body_ct,state,sent_at,expires_at,sender_idempotency_key,request_hash) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,'delivered',$9,$10,$11,$12)", c, tx);
        cmd.Parameters.AddWithValue(id); cmd.Parameters.AddWithValue(realm); cmd.Parameters.AddWithValue(senderAccount);
        cmd.Parameters.AddWithValue(senderCharacter); cmd.Parameters.AddWithValue(recipientAccount); cmd.Parameters.AddWithValue(recipientCharacter);
        cmd.Parameters.AddWithValue(subjectCt); cmd.Parameters.AddWithValue(bodyCt); cmd.Parameters.AddWithValue(now);
        cmd.Parameters.AddWithValue(now.AddDays(controls.ExpiryDays)); cmd.Parameters.AddWithValue(idempotencyKey); cmd.Parameters.AddWithValue(requestHash);
        cmd.ExecuteNonQuery(); tx.Commit(); return id;
    }

    public IReadOnlyList<StoredMail> Inbox(string recipient, int limit = 100)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT mail_id,sender_account_id,sender_character_id,subject_ct,body_ct,state,sent_at,read_at FROM chat.mail_messages WHERE recipient_character_id=$1 AND state IN ('delivered','read') AND expires_at>now() ORDER BY sent_at DESC,mail_id DESC LIMIT $2", c);
        cmd.Parameters.AddWithValue(recipient); cmd.Parameters.AddWithValue(limit);
        using var r = cmd.ExecuteReader(); var rows = new List<StoredMail>();
        while (r.Read()) rows.Add(new(r.GetString(0),r.GetString(1),r.GetString(2),r.GetFieldValue<byte[]>(3),r.GetFieldValue<byte[]>(4),r.GetString(5),r.GetFieldValue<DateTimeOffset>(6),r.IsDBNull(7)?null:r.GetFieldValue<DateTimeOffset>(7)));
        return rows;
    }

    public StoredMail? Find(string recipient, string mailId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("SELECT mail_id,sender_account_id,sender_character_id,subject_ct,body_ct,state,sent_at,read_at FROM chat.mail_messages WHERE recipient_character_id=$1 AND mail_id=$2 AND state IN ('delivered','read') AND expires_at>now()", c);
        cmd.Parameters.AddWithValue(recipient); cmd.Parameters.AddWithValue(mailId);
        using var r = cmd.ExecuteReader();
        return r.Read() ? new(r.GetString(0),r.GetString(1),r.GetString(2),r.GetFieldValue<byte[]>(3),r.GetFieldValue<byte[]>(4),r.GetString(5),r.GetFieldValue<DateTimeOffset>(6),r.IsDBNull(7)?null:r.GetFieldValue<DateTimeOffset>(7)) : null;
    }

    public void Reencrypt(string recipient, string mailId, byte[] subject, byte[] body)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("UPDATE chat.mail_messages SET subject_ct=$3,body_ct=$4,version=version+1 WHERE recipient_character_id=$1 AND mail_id=$2 AND state IN ('delivered','read')", c);
        cmd.Parameters.AddWithValue(recipient); cmd.Parameters.AddWithValue(mailId); cmd.Parameters.AddWithValue(subject); cmd.Parameters.AddWithValue(body);
        if (cmd.ExecuteNonQuery()!=1) throw new InvalidOperationException("MAIL_NOT_FOUND");
    }

    public bool MarkRead(string recipient, string mailId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("UPDATE chat.mail_messages SET state='read',read_at=COALESCE(read_at,now()),version=version+1 WHERE recipient_character_id=$1 AND mail_id=$2 AND state IN ('delivered','read')", c);
        cmd.Parameters.AddWithValue(recipient); cmd.Parameters.AddWithValue(mailId); return cmd.ExecuteNonQuery()==1;
    }

    public bool Delete(string recipient, string mailId)
    {
        using var c = Open();
        using var cmd = new NpgsqlCommand("UPDATE chat.mail_messages SET state='deleted',subject_ct='\\x'::bytea,body_ct='\\x'::bytea,version=version+1 WHERE recipient_character_id=$1 AND mail_id=$2 AND state IN ('delivered','read')", c);
        cmd.Parameters.AddWithValue(recipient); cmd.Parameters.AddWithValue(mailId); return cmd.ExecuteNonQuery()==1;
    }
}
