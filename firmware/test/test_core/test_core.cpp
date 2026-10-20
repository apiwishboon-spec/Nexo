// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
// Host-side unit tests for the portable Nexo core (no hardware required).
// Run with: cd firmware && pio test -e native
#include <unity.h>

#include <map>
#include <string>
#include <vector>

#include "nexo_codec.h"
#include "nexo_frame.h"
#include "nexo_menu.h"
#include "nexo_calc.h"
#include "nexo_protocol.h"
#include "nexo_repository.h"
#include "nexo_service.h"
#include "nexo_storage.h"
#include "nexo_sync.h"
#include "nexo_types.h"

using namespace nexo;

namespace {

class MemoryStore : public IStore {
public:
    bool load(const std::string& name, std::string& out) override
    {
        auto it = files_.find(name);
        if (it == files_.end()) return false;
        out = it->second;
        return true;
    }
    bool save(const std::string& name, const std::string& data) override
    {
        files_[name] = data;
        ++saveCalls_;
        return true;
    }
    std::map<std::string, std::string> files_;
    int saveCalls_ = 0;
};

// Deterministic clock + id source for tests.
uint32_t g_now = 1000;
uint32_t fakeNow() { return g_now; }
int g_idCounter = 0;
std::string fakeId()
{
    static const char* hex = "0123456789abcdef";
    std::string s = "0000000000000000";
    int v = ++g_idCounter;
    for (int i = 0; i < 8; ++i) s[15 - i] = hex[(v >> (4 * i)) & 0xF];
    return s;
}

struct Fixture {
    MemoryStore store;
    Repository repo;
    Fixture() : repo(store, fakeNow, fakeId) { repo.begin(); }
};

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ---- types -----------------------------------------------------------------
void test_id_validation()
{
    TEST_ASSERT_TRUE(isValidId("0123456789abcdef"));
    TEST_ASSERT_FALSE(isValidId("0123456789ABCDEF"));
    TEST_ASSERT_FALSE(isValidId("short"));
    TEST_ASSERT_FALSE(isValidId("0123456789abcde"));
}

void test_clamp_utf8()
{
    TEST_ASSERT_EQUAL_STRING("hello", clampUtf8("hello", 10).c_str());
    // Multibyte 'é' is 2 bytes; clamping must not split it.
    std::string s = "a\xC3\xA9";
    TEST_ASSERT_EQUAL_STRING("a", clampUtf8(s, 2).c_str());
}

// ---- repository ------------------------------------------------------------
void test_put_note_create_and_conflict()
{
    Fixture f;
    Note n;
    n.title = "Shopping";
    n.body = "milk";
    Note stored;
    TEST_ASSERT_TRUE(f.repo.putNote(n, 0, stored) == RepoStatus::Ok);
    TEST_ASSERT_TRUE(isValidId(stored.id));
    TEST_ASSERT_EQUAL_UINT32(1, stored.rev);

    // Update with correct baseRev.
    Note up = stored;
    up.title = "Shopping list";
    Note stored2;
    TEST_ASSERT_TRUE(f.repo.putNote(up, 1, stored2) == RepoStatus::Ok);
    TEST_ASSERT_EQUAL_UINT32(2, stored2.rev);

    // Stale baseRev -> conflict, returns current.
    Note stale = stored;  // rev 1
    stale.title = "wrong";
    Note conflictOut;
    TEST_ASSERT_TRUE(f.repo.putNote(stale, 1, conflictOut) == RepoStatus::Conflict);
    TEST_ASSERT_EQUAL_UINT32(2, conflictOut.rev);
    TEST_ASSERT_EQUAL_STRING("Shopping list", conflictOut.title.c_str());
}

void test_persistence_roundtrip()
{
    MemoryStore store;
    {
        Repository repo(store, fakeNow, fakeId);
        repo.begin();
        Note n;
        n.title = "Persist";
        Note out;
        repo.putNote(n, 0, out);
    }
    Repository repo2(store, fakeNow, fakeId);
    repo2.begin();
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)repo2.notes().size());
    TEST_ASSERT_EQUAL_STRING("Persist", repo2.notes()[0].title.c_str());
}

void test_delete_is_tombstone()
{
    Fixture f;
    Note n;
    n.title = "x";
    Note stored;
    f.repo.putNote(n, 0, stored);
    Note out;
    TEST_ASSERT_TRUE(f.repo.deleteNote(stored.id, stored.rev, out) == RepoStatus::Ok);
    TEST_ASSERT_TRUE(out.deleted);
    // Listing skips tombstones.
    Note got;
    TEST_ASSERT_TRUE(f.repo.getNote(stored.id, got));
    TEST_ASSERT_TRUE(got.deleted);
}

void test_validation_rejects_oversized()
{
    Fixture f;
    Note n;
    n.title = std::string(kMaxTitleLen + 5, 'a');
    Note out;
    // putNote clamps rather than rejects.
    TEST_ASSERT_TRUE(f.repo.putNote(n, 0, out) == RepoStatus::Ok);
    TEST_ASSERT_TRUE(out.title.size() <= kMaxTitleLen);
}

// ---- codec -----------------------------------------------------------------
void test_note_codec_roundtrip()
{
    Note n;
    n.id = "0123456789abcdef";
    n.title = "Title";
    n.body = "Body text";
    n.created = 10;
    n.updated = 20;
    n.rev = 3;
    ByteWriter w;
    encodeNote(w, n, 2);
    ByteReader r(w.data());
    Note out;
    uint32_t base = 0;
    TEST_ASSERT_TRUE(decodeNote(r, out, base));
    TEST_ASSERT_EQUAL_UINT32(2, base);
    TEST_ASSERT_EQUAL_STRING(n.id.c_str(), out.id.c_str());
    TEST_ASSERT_EQUAL_STRING(n.title.c_str(), out.title.c_str());
    TEST_ASSERT_EQUAL_UINT32(3, out.rev);
}

void test_homework_codec_status_clamp()
{
    Homework h;
    h.id = "0123456789abcdef";
    h.subject = "Math";
    h.status = static_cast<HomeworkStatus>(9);  // invalid -> clamp to Done
    ByteWriter w;
    encodeHomework(w, h, 0);
    ByteReader r(w.data());
    Homework out;
    uint32_t base;
    TEST_ASSERT_TRUE(decodeHomework(r, out, base));
    TEST_ASSERT_EQUAL_UINT8(2, (uint8_t)out.status);
}

// ---- framing ---------------------------------------------------------------
void test_frame_single_chunk()
{
    Message m;
    m.type = OP_PING;
    m.id = 7;
    m.payload = "hi";
    std::vector<std::string> chunks;
    TEST_ASSERT_TRUE(encodeMessage(m, 64, chunks));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)chunks.size());
    Reassembler re;
    Message out;
    TEST_ASSERT_TRUE(re.feed((const uint8_t*)chunks[0].data(), chunks[0].size(), out));
    TEST_ASSERT_EQUAL_UINT8(OP_PING, out.type);
    TEST_ASSERT_EQUAL_UINT16(7, out.id);
    TEST_ASSERT_EQUAL_STRING("hi", out.payload.c_str());
}

void test_frame_multi_chunk()
{
    Message m;
    m.type = OP_SYNC_PULL;
    m.id = 300;
    m.payload = std::string(500, 'Z');
    std::vector<std::string> chunks;
    TEST_ASSERT_TRUE(encodeMessage(m, 64, chunks));
    TEST_ASSERT_TRUE(chunks.size() > 1);
    Reassembler re;
    Message out;
    bool done = false;
    for (auto& c : chunks) {
        if (re.feed((const uint8_t*)c.data(), c.size(), out)) done = true;
    }
    TEST_ASSERT_TRUE(done);
    TEST_ASSERT_EQUAL_UINT32(500, (uint32_t)out.payload.size());
    TEST_ASSERT_EQUAL_UINT8('Z', (uint8_t)out.payload[499]);
}

void test_frame_rejects_oversized()
{
    std::string bad(8, '\0');
    bad[0] = 1;
    bad[1] = OP_PING;
    // total length = 9999 (> kMaxMessage)
    bad[6] = (char)(9999 & 0xFF);
    bad[7] = (char)((9999 >> 8) & 0xFF);
    Reassembler re;
    Message out;
    TEST_ASSERT_FALSE(re.feed((const uint8_t*)bad.data(), bad.size(), out));
    TEST_ASSERT_EQUAL_UINT8(ST_TOO_LARGE, re.lastError());
}

// ---- sync ------------------------------------------------------------------
void test_sync_pull_and_push()
{
    Fixture f;
    SyncEngine sync(f.repo);
    // Create two notes.
    Note a;
    a.title = "A";
    Note oa;
    f.repo.putNote(a, 0, oa);
    g_now += 10;
    Note b;
    b.title = "B";
    Note ob;
    f.repo.putNote(b, 0, ob);

    // Pull everything since 0.
    SyncDelta delta = sync.buildDelta(0, 0, 100);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)delta.kinds.size());

    // Push an update from a client.
    SyncPush push;
    push.kinds.push_back(1);
    Note upd = oa;
    upd.title = "A2";
    upd.rev = oa.rev;  // baseRev
    push.notes.push_back(upd);
    SyncResult res = sync.applyPush(push, g_now);
    TEST_ASSERT_EQUAL_UINT16(1, res.applied);
    TEST_ASSERT_EQUAL_UINT16(0, res.conflicts);
    Note check;
    f.repo.getNote(oa.id, check);
    TEST_ASSERT_EQUAL_STRING("A2", check.title.c_str());
}

void test_sync_conflict_detected()
{
    Fixture f;
    SyncEngine sync(f.repo);
    Note a;
    a.title = "A";
    Note oa;
    f.repo.putNote(a, 0, oa);  // rev 1

    // Client A updates to rev 2.
    SyncPush p1;
    p1.kinds.push_back(1);
    Note client = oa;
    client.title = "A-client";
    client.rev = 1;
    p1.notes.push_back(client);
    sync.applyPush(p1, g_now);

    // Client B tries to update from the now-stale rev 1.
    SyncPush p2;
    p2.kinds.push_back(1);
    Note clientB = oa;
    clientB.title = "A-other";
    clientB.rev = 1;
    p2.notes.push_back(clientB);
    SyncResult r2 = sync.applyPush(p2, g_now);
    TEST_ASSERT_EQUAL_UINT16(0, r2.applied);
    TEST_ASSERT_EQUAL_UINT16(1, r2.conflicts);
    TEST_ASSERT_EQUAL_STRING("A-client", r2.entries[0].note.title.c_str());
}

// ---- service ---------------------------------------------------------------
void test_service_requires_auth()
{
    Fixture f;
    SyncEngine sync(f.repo);
    g_now = 1000;
    NexoService svc(f.repo, sync, fakeNow);

    Message req;
    req.version = kProtocolVersion;
    req.type = OP_LIST_NOTES;
    req.id = 1;
    ByteWriter w;
    w.u16(0);
    w.u16(10);
    req.payload = w.data();
    Message resp = svc.handle(req);
    TEST_ASSERT_EQUAL_UINT8(ST_UNAUTHORIZED, (uint8_t)resp.payload[0]);

    // Auth with default pin.
    Message auth;
    auth.version = kProtocolVersion;
    auth.type = OP_AUTH;
    auth.id = 2;
    std::string pin = "123456";
    auth.payload.push_back((char)pin.size());
    auth.payload += pin;
    Message ar = svc.handle(auth);
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)ar.payload[0]);
    TEST_ASSERT_TRUE(svc.authenticated());

    // Now list works (new request id, since reusing id 1 would be a dedupe hit).
    req.id = 3;
    Message lr = svc.handle(req);
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)lr.payload[0]);
}

void test_service_bad_pin_locks()
{
    Fixture f;
    SyncEngine sync(f.repo);
    g_now = 1000;
    NexoService svc(f.repo, sync, fakeNow);
    for (int i = 0; i < 5; ++i) {
        Message auth;
        auth.version = kProtocolVersion;
        auth.type = OP_AUTH;
        auth.id = (uint16_t)(10 + i);
        std::string pin = "0000";
        auth.payload.push_back((char)pin.size());
        auth.payload += pin;
        svc.handle(auth);
    }
    // Sixth attempt is locked.
    Message auth;
    auth.version = kProtocolVersion;
    auth.type = OP_AUTH;
    auth.id = 99;
    std::string pin = "123456";
    auth.payload.push_back((char)pin.size());
    auth.payload += pin;
    Message r = svc.handle(auth);
    TEST_ASSERT_EQUAL_UINT8(ST_LOCKED, (uint8_t)r.payload[0]);
}

void test_service_duplicate_request_replay()
{
    Fixture f;
    SyncEngine sync(f.repo);
    g_now = 1000;
    NexoService svc(f.repo, sync, fakeNow);
    Message auth;
    auth.type = OP_AUTH;
    auth.id = 1;
    auth.payload.push_back((char)6);
    auth.payload += "123456";
    svc.handle(auth);

    Message ping;
    ping.version = kProtocolVersion;
    ping.type = OP_PING;
    ping.id = 5;
    Message r1 = svc.handle(ping);
    Message r2 = svc.handle(ping);  // replay
    TEST_ASSERT_EQUAL_STRING(r1.payload.c_str(), r2.payload.c_str());
}

void test_service_version_mismatch()
{
    Fixture f;
    SyncEngine sync(f.repo);
    NexoService svc(f.repo, sync, fakeNow);
    Message m;
    m.version = 99;
    m.type = OP_PING;
    m.id = 1;
    Message r = svc.handle(m);
    TEST_ASSERT_EQUAL_UINT8(ST_VERSION, (uint8_t)r.payload[0]);
}

void test_calc_expression()
{
    double v = 0;
    TEST_ASSERT_TRUE(evalExpression("2+3*4", v));
    TEST_ASSERT_EQUAL_FLOAT(14.0f, (float)v);
    TEST_ASSERT_TRUE(evalExpression("(2+3)*4", v));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, (float)v);
    TEST_ASSERT_TRUE(evalExpression("10/4", v));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, (float)v);
    TEST_ASSERT_FALSE(evalExpression("2+", v));
    TEST_ASSERT_FALSE(evalExpression("1/0", v));
    TEST_ASSERT_FALSE(evalExpression("abc", v));
}

void test_due_date_roundtrip()
{
    TEST_ASSERT_EQUAL_UINT32(0, isoToDueDay(""));
    TEST_ASSERT_EQUAL_UINT32(0, isoToDueDay("bogus"));
    uint32_t day = isoToDueDay("2026-10-14");
    TEST_ASSERT_TRUE(day > 0);
    TEST_ASSERT_EQUAL_STRING("2026-10-14", dueDayToIso(day).c_str());
    TEST_ASSERT_EQUAL_STRING("", dueDayToIso(0).c_str());
    TEST_ASSERT_EQUAL_STRING("1970-01-01", dueDayToIso(1).c_str());
}

void test_batch_persist_coalesces()
{
    Fixture f;
    const int before = f.store.saveCalls_;
    f.repo.beginBatch();
    for (int i = 0; i < 5; ++i) {
        Note n;
        n.title = "t";
        Note stored;
        TEST_ASSERT_TRUE(f.repo.putNote(n, 0, stored) == RepoStatus::Ok);
    }
    // Nothing written to storage while the batch is open.
    TEST_ASSERT_EQUAL_INT(before, f.store.saveCalls_);
    TEST_ASSERT_TRUE(f.repo.endBatch());
    // A single flush for all five records.
    TEST_ASSERT_EQUAL_INT(before + 1, f.store.saveCalls_);
    TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)f.repo.notes().size());
}

// ---- v2 --------------------------------------------------------------------
void test_settings_extended_roundtrip()
{
    Fixture f;
    Settings s = f.repo.settings();
    s.deviceName = "Desk";
    s.volume = 42;
    s.soundEnabled = false;
    s.brightness = 70;
    s.screenTimeoutSec = 45;
    s.autoAdvanceMusic = false;
    s.timezoneOffsetMin = 420;  // UTC+7
    TEST_ASSERT_TRUE(f.repo.applySettings(s, false) == RepoStatus::Ok);

    ByteWriter w;
    encodeSettings(w, f.repo.settings(), false);
    Settings out;
    ByteReader r(w.data());
    TEST_ASSERT_TRUE(decodeSettings(r, out));
    TEST_ASSERT_EQUAL_STRING("Desk", out.deviceName.c_str());
    TEST_ASSERT_EQUAL_UINT8(42, out.volume);
    TEST_ASSERT_FALSE(out.soundEnabled);
    TEST_ASSERT_EQUAL_UINT8(70, out.brightness);
    TEST_ASSERT_EQUAL_UINT16(45, out.screenTimeoutSec);
    TEST_ASSERT_FALSE(out.autoAdvanceMusic);
    TEST_ASSERT_EQUAL_INT16(420, out.timezoneOffsetMin);
}

void test_device_info_encode_decode()
{
    DeviceInfo d;
    d.deviceName = "Nexo";
    d.firmwareVersion = "2.0.0";
    d.batteryPercent = 88;
    d.charging = true;
    d.uptimeSec = 1234;
    d.sdPresent = true;
    d.musicCount = 3;
    d.volume = 60;
    d.headphoneIn = false;
    d.apEnabled = true;
    std::string enc = encodeDeviceInfo(d);
    // Sanity: starts with the proto tag and ends with the terminator.
    TEST_ASSERT_EQUAL_UINT8((uint8_t)kDevProto, (uint8_t)enc[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)kDevEnd, (uint8_t)enc[enc.size() - 1]);
    TEST_ASSERT_TRUE(enc.find("2.0.0") != std::string::npos);
}

void test_music_status_encode()
{
    MusicStatus st;
    st.index = 3;
    st.playing = true;
    st.positionSec = 12;
    st.durationSec = 200;
    st.volume = 55;
    std::string enc = encodeMusicStatus(st);
    ByteReader r(enc);
    uint16_t idx = 0, pos = 0, dur = 0;
    uint8_t playing = 0, vol = 0;
    TEST_ASSERT_TRUE(r.u16(idx));
    TEST_ASSERT_TRUE(r.u8(playing));
    TEST_ASSERT_TRUE(r.u16(pos));
    TEST_ASSERT_TRUE(r.u16(dur));
    TEST_ASSERT_TRUE(r.u8(vol));
    TEST_ASSERT_EQUAL_INT16(3, (int16_t)idx);
    TEST_ASSERT_TRUE(playing == 1);
    TEST_ASSERT_EQUAL_UINT16(12, pos);
    TEST_ASSERT_EQUAL_UINT16(200, dur);
    TEST_ASSERT_EQUAL_UINT8(55, vol);
}

// Mock hardware bridge so device ops are exercised end-to-end.
class MockHooks : public DeviceHooks {
public:
    DeviceInfo info() override
    {
        DeviceInfo d;
        d.deviceName = "Mock";
        d.batteryPercent = 50;
        return d;
    }
    void setTime(uint32_t epoch, int16_t tz) override
    {
        lastEpoch = epoch;
        lastTz = tz;
    }
    void reboot() override { rebooted = true; }
    void find() override { found = true; }
    std::vector<MusicTrack> listMusic() override
    {
        return {{"a.mp3", 100}, {"b.mp3", 200}};
    }
    MusicStatus nowPlaying() override
    {
        MusicStatus s;
        s.index = 1;
        s.playing = true;
        return s;
    }
    RepoStatus musicControl(MusicAction action, int16_t index, uint16_t) override
    {
        lastAction = action;
        lastIndex = index;
        return RepoStatus::Ok;
    }
    uint32_t lastEpoch = 0;
    int16_t lastTz = 0;
    bool rebooted = false;
    bool found = false;
    MusicAction lastAction = MUSIC_TOGGLE;
    int16_t lastIndex = -99;
};

void test_service_device_and_music_ops()
{
    Fixture f;
    SyncEngine sync(f.repo);
    g_now = 1500;
    MockHooks hooks;
    NexoService svc(f.repo, sync, fakeNow, &hooks);

    // Authenticate first (device ops require auth).
    Message auth;
    auth.version = kProtocolVersion;
    auth.type = OP_AUTH;
    auth.id = 1;
    std::string pin = "123456";
    auth.payload.push_back((char)pin.size());
    auth.payload += pin;
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)svc.handle(auth).payload[0]);

    // GET_DEVICE_INFO returns the mock data.
    Message info;
    info.version = kProtocolVersion;
    info.type = OP_GET_DEVICE_INFO;
    info.id = 2;
    Message ir = svc.handle(info);
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)ir.payload[0]);
    TEST_ASSERT_TRUE(ir.payload.find("Mock") != std::string::npos);

    // SET_TIME forwards the epoch + tz.
    Message st;
    st.version = kProtocolVersion;
    st.type = OP_SET_TIME;
    st.id = 3;
    ByteWriter sw;
    sw.u32(1700000000u);
    sw.u16((uint16_t)(int16_t)420);
    st.payload = sw.data();
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)svc.handle(st).payload[0]);
    TEST_ASSERT_EQUAL_UINT32(1700000000u, hooks.lastEpoch);
    TEST_ASSERT_EQUAL_INT16(420, hooks.lastTz);

    // FIND_DEVICE hits the hook.
    Message fd;
    fd.version = kProtocolVersion;
    fd.type = OP_FIND_DEVICE;
    fd.id = 4;
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)svc.handle(fd).payload[0]);
    TEST_ASSERT_TRUE(hooks.found);

    // LIST_MUSIC returns two tracks.
    Message lm;
    lm.version = kProtocolVersion;
    lm.type = OP_LIST_MUSIC;
    lm.id = 5;
    Message lmresp = svc.handle(lm);
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)lmresp.payload[0]);
    ByteReader lr(reinterpret_cast<const uint8_t*>(lmresp.payload.data()) + 1,
                  lmresp.payload.size() - 1);
    uint16_t count = 0;
    TEST_ASSERT_TRUE(lr.u16(count));
    TEST_ASSERT_EQUAL_UINT16(2, count);

    // MUSIC_CONTROL forwards play-index.
    Message mc;
    mc.version = kProtocolVersion;
    mc.type = OP_MUSIC_CONTROL;
    mc.id = 6;
    ByteWriter mw;
    mw.u8((uint8_t)MUSIC_PLAY_INDEX);
    mw.u16(1);
    mw.u16(0);
    mc.payload = mw.data();
    TEST_ASSERT_EQUAL_UINT8(ST_OK, (uint8_t)svc.handle(mc).payload[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MUSIC_PLAY_INDEX, (uint8_t)hooks.lastAction);
    TEST_ASSERT_EQUAL_INT16(1, hooks.lastIndex);
}

// Menu labels must stay bound to their intended destination regardless of the
// row order in which they are displayed (issue #1: Settings opened About).
void test_home_menu_labels_map_to_targets()
{
    const std::map<std::string, MenuTarget> expected = {
        {"Quick Notes", MenuTarget::Notes},
        {"Homework", MenuTarget::Homework},
        {"Calculator", MenuTarget::Calculator},
        {"Timer", MenuTarget::Timer},
        {"Memos", MenuTarget::Memos},
        {"Settings", MenuTarget::Settings},
        {"About", MenuTarget::About},
    };
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expected.size(), (uint32_t)kMenuEntryCount);

    std::map<std::string, int> seen;
    for (size_t i = 0; i < kMenuEntryCount; ++i) {
        MenuEntry e = kMenuEntries[i];
        TEST_ASSERT_NOT_NULL(e.label);
        auto it = expected.find(e.label);
        TEST_ASSERT_TRUE_MESSAGE(it != expected.end(), "menu label has no expected target");
        TEST_ASSERT_EQUAL_UINT8((uint8_t)it->second, (uint8_t)e.target);
        ++seen[e.label];
    }
    for (const auto& kv : seen) TEST_ASSERT_EQUAL_INT(1, kv.second);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_id_validation);
    RUN_TEST(test_clamp_utf8);
    RUN_TEST(test_put_note_create_and_conflict);
    RUN_TEST(test_persistence_roundtrip);
    RUN_TEST(test_delete_is_tombstone);
    RUN_TEST(test_validation_rejects_oversized);
    RUN_TEST(test_note_codec_roundtrip);
    RUN_TEST(test_homework_codec_status_clamp);
    RUN_TEST(test_frame_single_chunk);
    RUN_TEST(test_frame_multi_chunk);
    RUN_TEST(test_frame_rejects_oversized);
    RUN_TEST(test_sync_pull_and_push);
    RUN_TEST(test_sync_conflict_detected);
    RUN_TEST(test_service_requires_auth);
    RUN_TEST(test_service_bad_pin_locks);
    RUN_TEST(test_service_duplicate_request_replay);
    RUN_TEST(test_service_version_mismatch);
    RUN_TEST(test_calc_expression);
    RUN_TEST(test_due_date_roundtrip);
    RUN_TEST(test_batch_persist_coalesces);
    RUN_TEST(test_settings_extended_roundtrip);
    RUN_TEST(test_device_info_encode_decode);
    RUN_TEST(test_music_status_encode);
    RUN_TEST(test_service_device_and_music_ops);
    RUN_TEST(test_home_menu_labels_map_to_targets);
    return UNITY_END();
}
