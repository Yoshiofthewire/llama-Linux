# Pairing Auth: Query Params to Headers (Linux Client) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Switch every networking class in this app that sends pairing-auth
credentials (`subscriberId`/`subscriberHash`, held in `RelayAuth`) from
attaching them as `?sub=&hash=` URL query params to sending them as
`X-Kypost-Subscriber-Id`/`X-Kypost-Subscriber-Hash` HTTP headers, so the
credentials stop appearing in server access logs / reverse-proxy logs.

**Architecture:** `RelayAuth` (`core/net/RelayAuth.h`) gains a
`headerItems()` method alongside its existing `queryItems()`, returning the
same two values under the new header names. `HttpClient::get/post/put/del`
(`core/net/HttpClient.h`) already accept a `headers` parameter as their last
argument — added in a prior task, unused until now — so **no `HttpClient`
changes are needed at all**. Every call site that currently passes
`auth.queryItems()` as the `query` argument switches to passing `{}` (or
whatever non-auth query params it still needs) as `query` and
`auth.headerItems()` as `headers`. Once every call site is converted,
`queryItems()` is deleted — this is a clean cutover, matching the
already-completed Android client migration, not a dual-write; the server
(already shipped) accepts both forms, so the client doesn't need to.

**Tech Stack:** C++20, Qt6 (`QNetworkAccessManager`/`QNetworkRequest`), Qt
Test (`QVERIFY`/`QCOMPARE`, `QTEST_GUILESS_MAIN`), CMake/CTest. Test fakes
use `FakeRelayServer` (`tests/core/net/FakeRelayServer.h`), a real
`QTcpServer` on an ephemeral port that captures raw request bytes — this
codebase has no mocking framework and no `HttpClient` test double; every
`core/net/` test exercises real `HttpClient`/`QNetworkAccessManager`
end-to-end against this fake server.

## Global Constraints

- Build: `cmake --build build` must succeed with zero errors (this is also
  Task 8's proof that no stray call site still references the deleted
  `queryItems()`).
- Per-task verification: `cmake --build build --target <Name>Test &&
  ./build/tests/<Name>Test` for the specific test binary touched by that
  task (matches the convention in this repo's prior plans, e.g.
  `docs/superpowers/plans/2026-07-18-contact-self-and-pgp-qr-card.md`).
- Final verification: `ctest --test-dir build` must be fully green (per
  `AGENTS.md` Section 3: "A change is not verified until this builds
  cleanly and `ctest` is green").
- Header names are exactly `X-Kypost-Subscriber-Id` and
  `X-Kypost-Subscriber-Hash`, matching what the server (already shipped)
  and the Android client (already shipped) use.
- This is a clean cutover, not a dual-write: the client sends headers only,
  no `?sub=&hash=` fallback. The server accepts both (already shipped), so
  this is safe — no coordination window needed on the client side.
- Scope is exactly: `RelayAuth.h` plus the 6 files with `queryItems()` call
  sites (`PgpQrClient.cpp`, `ContactPhotoClient.cpp`,
  `PushNotificationClient.cpp`, `RelayMailSource.cpp`, `GroupsClient.cpp`,
  `ContactSyncClient.cpp`). Out of scope, do not touch:
  `PgpQrClient::fetchKey` (unrelated single-use `t` token, not sub/hash —
  takes a pre-built URL with the token already embedded and passes an empty
  query), `NativeRegistrationClient.*` and `MfaResponseClient.*` (both
  already send `subscriberId`/`subscriberHash` in the JSON POST body, never
  touch `RelayAuth`/`queryItems()` at all — confirmed by reading both
  files), and `HttpClient.h`/`HttpClient.cpp` (the `headers` parameter this
  plan uses already exists and is already correct; no change needed there).
- Every class/method whose only change is the auth transport must not have
  its other query params (e.g. `RelayMailSource::fetchInbox`'s
  `limit`/`mailbox`/`since`, `PushNotificationClient::pull`'s conditional
  `after`) dropped or altered — only `sub`/`hash` move.
- `RelayAuth.h`'s doc comment references a path, `llama-Mail-for-Mac`, that
  doesn't match the actual directory on this machine
  (`~/git/kypost-for-Mac`) — a pre-existing naming mismatch, not something
  this plan introduces or is responsible for fixing. Leave it as-is; not in
  scope.

---

### Task 1: `RelayAuth::headerItems()` + its own test

**Files:**
- Modify: `core/net/RelayAuth.h`
- Create: `tests/core/net/RelayAuthTest.cpp` (new file)
- Modify: `tests/CMakeLists.txt` (register the new test target)

**Interfaces:**
- Consumes: nothing new.
- Produces: `RelayAuth::headerItems() const -> QList<QPair<QString,
  QString>>`, returning `{ {"X-Kypost-Subscriber-Id", sub},
  {"X-Kypost-Subscriber-Hash", hash} }`. Tasks 2–7 each call this method by
  name, passing its result as the `headers` argument to
  `HttpClient::get/post/put/del`. `queryItems()` is left in place for now —
  Tasks 2–7 still need it removed from their own call sites one file at a
  time; Task 8 deletes it once nothing calls it anymore.

- [ ] **Step 1: Write the failing test**

Create `tests/core/net/RelayAuthTest.cpp`:

```cpp
#include "net/RelayAuth.h"

#include <QTest>

class RelayAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void headerItemsReturnsSubscriberIdAndHashAsNamedHeaders();
};

void RelayAuthTest::headerItemsReturnsSubscriberIdAndHashAsNamedHeaders()
{
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };

    const QList<QPair<QString, QString>> items = auth.headerItems();

    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).first, QStringLiteral("X-Kypost-Subscriber-Id"));
    QCOMPARE(items.at(0).second, QStringLiteral("sub-1"));
    QCOMPARE(items.at(1).first, QStringLiteral("X-Kypost-Subscriber-Hash"));
    QCOMPARE(items.at(1).second, QStringLiteral("hash-1"));
}

QTEST_GUILESS_MAIN(RelayAuthTest)
#include "RelayAuthTest.moc"
```

In `tests/CMakeLists.txt`, add a new line immediately after the existing
`llama_add_test(NetworkErrorTest core/net/NetworkErrorTest.cpp)` line
(inside the `core/net/` block, before `HttpClientTest`):

```cmake
llama_add_test(RelayAuthTest core/net/RelayAuthTest.cpp)
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target RelayAuthTest`
Expected: FAIL to compile — `headerItems()` is undefined on `RelayAuth`.

- [ ] **Step 3: Write the implementation**

In `core/net/RelayAuth.h`, replace the whole file with:

```cpp
#pragma once

#include <QList>
#include <QPair>
#include <QString>

// Relay auth credentials (subscriber id + hash). headerItems() sends them
// as X-Kypost-Subscriber-Id/X-Kypost-Subscriber-Hash headers -- the form
// every Relay call site is migrating to (server already accepts both
// forms, headers preferred). queryItems() (legacy ?sub=&hash= query
// params) is being phased out call site by call site and deleted once
// nothing uses it anymore. Mirrors llama-Mail-for-Mac's RelayAuth
// (Data/Networking/HTTPClient.swift), read for structure only. Plain value
// type with no store dependency -- callers pull sub/hash out of whatever
// pairing/session store owns them and hand this to HttpClient::get/post.
struct RelayAuth
{
    QString sub;
    QString hash;

    QList<QPair<QString, QString>> queryItems() const
    {
        return { { QStringLiteral("sub"), sub }, { QStringLiteral("hash"), hash } };
    }

    QList<QPair<QString, QString>> headerItems() const
    {
        return { { QStringLiteral("X-Kypost-Subscriber-Id"), sub },
                 { QStringLiteral("X-Kypost-Subscriber-Hash"), hash } };
    }

    bool operator==(const RelayAuth&) const = default;
};
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target RelayAuthTest && ./build/tests/RelayAuthTest`
Expected: PASS (1/1 test)

- [ ] **Step 5: Commit**

```bash
git add core/net/RelayAuth.h tests/core/net/RelayAuthTest.cpp tests/CMakeLists.txt
git commit -m "net: add RelayAuth::headerItems() for the pairing-auth header migration"
```

---

### Task 2: `PgpQrClient.cpp` — `fetchToken` switches to headers

**Files:**
- Modify: `core/net/PgpQrClient.cpp`
- Test: `tests/core/net/PgpQrClientTest.cpp`

**Interfaces:**
- Consumes: `RelayAuth::headerItems()` from Task 1.
- Produces: no interface change. `fetchToken`'s signature and
  `PgpQrTokenResult` are unchanged. `fetchKey` is untouched (out of scope).

- [ ] **Step 1: Write the failing test**

In `tests/core/net/PgpQrClientTest.cpp`, rename the `private slots:`
declaration and the test definition from
`fetchTokenSendsAuthAsQueryParamsAndHitsApiPgpQrToken` to
`fetchTokenSendsAuthAsHeadersAndHitsApiPgpQrToken` (the old name becomes
inaccurate once this lands), and replace its body:

```cpp
void PgpQrClientTest::fetchTokenSendsAuthAsHeadersAndHitsApiPgpQrToken()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"token":"t","expiresAt":"","url":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.fetchToken(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/pgp/qr/token HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}
```

(The request line changes from `"GET /api/pgp/qr/token?"` to `"GET
/api/pgp/qr/token HTTP/1.1"` because this endpoint has no other query
params — once `sub`/`hash` move to headers, the URL has no query string at
all. `HttpClient::urlWithQuery` returns the URL unchanged when the query
list is empty, confirmed in `core/net/HttpClient.cpp`.)

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target PgpQrClientTest`
Expected: FAIL to compile (renamed test method doesn't match the old
declaration) or, once the rename is applied consistently, FAIL at runtime
(headers absent, `sub=`/`hash=` still present in the query string).

- [ ] **Step 3: Rewrite the production code**

In `core/net/PgpQrClient.cpp`, replace:
```cpp
    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/pgp/qr/token")), auth.queryItems());
```
with:
```cpp
    const HttpClient::HttpResult result = m_httpClient.get(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/pgp/qr/token")), {}, auth.headerItems());
```

`fetchKey` (below `fetchToken` in this file) is unchanged.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target PgpQrClientTest && ./build/tests/PgpQrClientTest`
Expected: PASS (all tests, including the untouched `fetchKey_*` tests)

- [ ] **Step 5: Commit**

```bash
git add core/net/PgpQrClient.cpp tests/core/net/PgpQrClientTest.cpp
git commit -m "net: send pairing auth as headers in PgpQrClient::fetchToken"
```

---

### Task 3: `ContactPhotoClient.cpp` — `fetch` switches to headers

**Files:**
- Modify: `core/net/ContactPhotoClient.cpp`
- Test: `tests/core/net/ContactPhotoClientTest.cpp`

**Interfaces:**
- Consumes: `RelayAuth::headerItems()` from Task 1.
- Produces: no interface change.

- [ ] **Step 1: Write the failing test**

In `tests/core/net/ContactPhotoClientTest.cpp`, rename
`fetchSendsAuthAsQueryParamsAndBuildsPathWithContactUid` (declaration and
definition) to `fetchSendsAuthAsHeadersAndBuildsPathWithContactUid`, and
replace its body:

```cpp
void ContactPhotoClientTest::fetchSendsAuthAsHeadersAndBuildsPathWithContactUid()
{
    FakeRelayServer fake(httpResponse(200, "OK", ""));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.fetch(serverBaseUrl, QStringLiteral("contact-42"), auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/contacts/contact-42/photo HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target ContactPhotoClientTest`
Expected: FAIL (compile error from the rename, then runtime failure once
consistent — headers absent).

- [ ] **Step 3: Rewrite the production code**

In `core/net/ContactPhotoClient.cpp`, replace:
```cpp
    const HttpClient::HttpResult result =
        m_httpClient.get(endpointFor(serverBaseUrl, contactUid), auth.queryItems());
```
with:
```cpp
    const HttpClient::HttpResult result =
        m_httpClient.get(endpointFor(serverBaseUrl, contactUid), {}, auth.headerItems());
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target ContactPhotoClientTest && ./build/tests/ContactPhotoClientTest`
Expected: PASS (all tests)

- [ ] **Step 5: Commit**

```bash
git add core/net/ContactPhotoClient.cpp tests/core/net/ContactPhotoClientTest.cpp
git commit -m "net: send pairing auth as headers in ContactPhotoClient::fetch"
```

---

### Task 4: `PushNotificationClient.cpp` — `pull` switches to headers

**Files:**
- Modify: `core/net/PushNotificationClient.cpp`
- Test: `tests/core/net/PushNotificationClientTest.cpp`

**Interfaces:**
- Consumes: `RelayAuth::headerItems()` from Task 1.
- Produces: no interface change. The conditional `after` query param
  (`afterCursor > 0`) is unaffected — it's not part of auth and stays in
  `query`.

- [ ] **Step 1: Write the failing test**

In `tests/core/net/PushNotificationClientTest.cpp`, update
`firstPullOmitsAfterQueryParam`'s assertion block from:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("sub=sub-1"));
    QVERIFY(request.contains("hash=hash-1"));
    QVERIFY(!request.contains("after="));
```
to:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-1"));
    QVERIFY(!request.contains("sub=sub-1"));
    QVERIFY(!request.contains("hash=hash-1"));
    QVERIFY(!request.contains("after="));
```

`subsequentPullSendsAfterQueryParam` (the `afterCursor = 42` case) is
unaffected — it only asserts `after=42`, which still travels as a query
param — leave it unmodified.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target PushNotificationClientTest`
Expected: `firstPullOmitsAfterQueryParam` FAILs (headers absent,
`sub=`/`hash=` still present); `subsequentPullSendsAfterQueryParam` still
PASSes (unaffected).

- [ ] **Step 3: Rewrite the production code**

In `core/net/PushNotificationClient.cpp`, replace:
```cpp
    QList<QPair<QString, QString>> query = auth.queryItems();
    if (afterCursor > 0)
        query.append({ QStringLiteral("after"), QString::number(afterCursor) });

    const HttpClient::HttpResult result = m_httpClient.get(pullEndpoint, query);
```
with:
```cpp
    QList<QPair<QString, QString>> query;
    if (afterCursor > 0)
        query.append({ QStringLiteral("after"), QString::number(afterCursor) });

    const HttpClient::HttpResult result = m_httpClient.get(pullEndpoint, query, auth.headerItems());
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target PushNotificationClientTest && ./build/tests/PushNotificationClientTest`
Expected: PASS (all tests)

- [ ] **Step 5: Commit**

```bash
git add core/net/PushNotificationClient.cpp tests/core/net/PushNotificationClientTest.cpp
git commit -m "net: send pairing auth as headers in PushNotificationClient::pull"
```

---

### Task 5: `GroupsClient.cpp` — `fetch` switches to headers

**Files:**
- Modify: `core/net/GroupsClient.cpp`
- Test: `tests/core/net/GroupsClientTest.cpp`

**Interfaces:**
- Consumes: `RelayAuth::headerItems()` from Task 1.
- Produces: no interface change.

- [ ] **Step 1: Write the failing test**

In `tests/core/net/GroupsClientTest.cpp`, rename
`fetchSendsAuthAsQueryParamsAndGetsCorrectPath` (declaration and
definition) to `fetchSendsAuthAsHeadersAndGetsCorrectPath`, and replace its
body:

```cpp
void GroupsClientTest::fetchSendsAuthAsHeadersAndGetsCorrectPath()
{
    FakeRelayServer fake(httpResponse(200, "OK", "[]"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    GroupsClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.fetch(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/groups HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target GroupsClientTest`
Expected: FAIL (compile error from rename, then runtime failure once
consistent).

- [ ] **Step 3: Rewrite the production code**

In `core/net/GroupsClient.cpp`, replace:
```cpp
    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/groups")), auth.queryItems());
```
with:
```cpp
    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/groups")), {}, auth.headerItems());
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target GroupsClientTest && ./build/tests/GroupsClientTest`
Expected: PASS (all tests)

- [ ] **Step 5: Commit**

```bash
git add core/net/GroupsClient.cpp tests/core/net/GroupsClientTest.cpp
git commit -m "net: send pairing auth as headers in GroupsClient::fetch"
```

---

### Task 6: `ContactSyncClient.cpp` — `pull`/`push`/`dedupe` switch to headers

**Files:**
- Modify: `core/net/ContactSyncClient.cpp`
- Test: `tests/core/net/ContactSyncClientTest.cpp`

**Interfaces:**
- Consumes: `RelayAuth::headerItems()` from Task 1.
- Produces: no interface change. `pull`'s `since` query param is
  unaffected — it's not part of auth and stays in `query`.

- [ ] **Step 1: Write the failing tests**

In `tests/core/net/ContactSyncClientTest.cpp`:

Rename `pullSendsSinceAndAuthAsQueryParams` (declaration + definition) to
`pullSendsSinceAsQueryParamAndAuthAsHeaders`, replacing its body:
```cpp
void ContactSyncClientTest::pullSendsSinceAsQueryParamAndAuthAsHeaders()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"cursor":0,"tooOld":false,"changed":[],"deleted":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.pull(serverBaseUrl, auth, 0);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/contacts/sync?"));
    QVERIFY(request.contains("since=0"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}
```
(`since` is always appended regardless of value, so the request line still
has a `?` — only the `sub=`/`hash=` pieces of the query string disappear.)

Rename `pushSendsBaseCursorAndAuthAsQueryParams` (declaration + definition)
to `pushSendsAuthAsHeaders`, replacing its body:
```cpp
void ContactSyncClientTest::pushSendsAuthAsHeaders()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"cursor":1,"tooOld":false,"changed":[],"deleted":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-7"), QStringLiteral("hash-7") };
    client.push(serverBaseUrl, auth, 0, {});

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-7"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-7"));
    QVERIFY(!request.contains("sub=sub-7"));
    QVERIFY(!request.contains("hash=hash-7"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("baseCursor")).toInt(), 0);
    QVERIFY(sent.value(QStringLiteral("changes")).toArray().isEmpty());
}
```

Rename `dedupeSendsAuthAsQueryParamsAndPostsToApiContactsDedupe`
(declaration + definition) to
`dedupeSendsAuthAsHeadersAndPostsToApiContactsDedupe`, replacing its body:
```cpp
void ContactSyncClientTest::dedupeSendsAuthAsHeadersAndPostsToApiContactsDedupe()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"mergedCount":0,"groups":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.dedupe(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("POST /api/contacts/dedupe HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
}
```
(`dedupe` has no other query params, so its request line loses the `?`
entirely once `sub`/`hash` move to headers — same reasoning as Tasks 2/3/5.)

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build --target ContactSyncClientTest`
Expected: the 3 renamed tests FAIL (headers absent, old query-param
assertions gone from the code so nothing to pass on that front, but the
new header assertions fail against the still-unconverted production code);
other tests in this file (e.g.
`pushRoundTripSendsExactFieldNamesIncludingEmptyUidCreate`,
`tooOldTrueSurfacesFlagWithEmptyChangedAndDeleted`) are unaffected.

- [ ] **Step 3: Rewrite the production code**

In `core/net/ContactSyncClient.cpp`, replace `pull`'s query/request
construction:
```cpp
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("since"), QString::number(since) });

    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), query);
```
with:
```cpp
    const QList<QPair<QString, QString>> query{ { QStringLiteral("since"), QString::number(since) } };

    const HttpClient::HttpResult result = m_httpClient.get(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), query, auth.headerItems());
```

Replace `push`'s request construction:
```cpp
    const HttpClient::HttpResult result =
        m_httpClient.post(joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), auth.queryItems(), body);
```
with:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/sync")), {}, body, auth.headerItems());
```

Replace `dedupe`'s request construction:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/dedupe")), auth.queryItems(), QJsonObject{});
```
with:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/contacts/dedupe")), {}, QJsonObject{}, auth.headerItems());
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build --target ContactSyncClientTest && ./build/tests/ContactSyncClientTest`
Expected: PASS (all tests)

- [ ] **Step 5: Commit**

```bash
git add core/net/ContactSyncClient.cpp tests/core/net/ContactSyncClientTest.cpp
git commit -m "net: send pairing auth as headers in ContactSyncClient (pull/push/dedupe)"
```

---

### Task 7: `RelayMailSource.cpp` — all 5 methods switch to headers

**Files:**
- Modify: `core/net/RelayMailSource.cpp`
- Test: `tests/core/net/RelayMailSourceTest.cpp`

**Interfaces:**
- Consumes: `RelayAuth::headerItems()` from Task 1.
- Produces: no interface change. Every non-auth query param
  (`fetchInbox`'s `limit`/`mailbox`/`since`, `listAttachments`'s
  `mailbox`/`messageId`, `downloadAttachment`'s
  `mailbox`/`messageId`/`index`) is unaffected — only `sub`/`hash` move.

This is the largest of the 6 client files: 5 methods across 2 shapes —
`fetchInbox`/`listAttachments`/`downloadAttachment` build a local `query`
list with `auth.queryItems()` plus extra params appended; `performAction`/
`sendMail` pass `auth.queryItems()` directly to `post()` with no other
query params (their data rides in the JSON body instead).

- [ ] **Step 1: Write the failing tests**

In `tests/core/net/RelayMailSourceTest.cpp`, rename
`fetchInboxSendsLimitMailboxSinceAndAuthAsQueryParams` (declaration +
definition) to `fetchInboxSendsLimitMailboxSinceAsQueryParamsAndAuthAsHeaders`,
replacing its assertion block from:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/inbox?"));
    QVERIFY(request.contains("sub=sub-9"));
    QVERIFY(request.contains("hash=hash-9"));
    QVERIFY(request.contains("limit=250"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("since=12345"));
```
to:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/inbox?"));
    QVERIFY(request.contains("limit=250"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("since=12345"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-9"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-9"));
    QVERIFY(!request.contains("sub=sub-9"));
    QVERIFY(!request.contains("hash=hash-9"));
```

Extend `performActionMoveIncludesTargetMailboxInRequestBody`'s assertion
block (add lines, keep the existing ones) from:
```cpp
    QVERIFY(fake.receivedRequest().contains("POST /api/inbox/actions?"));
    const QJsonObject sent = fake.receivedJsonBody();
```
to:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("POST /api/inbox/actions HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-1"));
    QVERIFY(!request.contains("sub=sub-1"));
    QVERIFY(!request.contains("hash=hash-1"));
    const QJsonObject sent = fake.receivedJsonBody();
```
(This endpoint has no other query params, so — same reasoning as Task
2/3/5/6's `dedupe` — the request line loses its `?` entirely once
`sub`/`hash` move to headers: `"POST /api/inbox/actions?"` becomes `"POST
/api/inbox/actions HTTP/1.1"`.)

Extend `sendMailJoinsRecipientsAndBase64EncodesAttachmentByteForByte`'s
assertion block from:
```cpp
    QVERIFY(fake.receivedRequest().contains("POST /api/mail/send?"));
    const QJsonObject sent = fake.receivedJsonBody();
```
to:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("POST /api/mail/send HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-1"));
    QVERIFY(!request.contains("sub=sub-1"));
    QVERIFY(!request.contains("hash=hash-1"));
    const QJsonObject sent = fake.receivedJsonBody();
```

Rename `listAttachmentsSendsMailboxMessageIdAndAuthAsQueryParamsAndParsesResult`
(declaration + definition) to
`listAttachmentsSendsMailboxMessageIdAsQueryParamsAndAuthAsHeadersAndParsesResult`,
replacing its assertion block from:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/mail/attachments?"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("messageId=42"));
    QVERIFY(request.contains("sub=sub-1"));
    QVERIFY(request.contains("hash=hash-1"));
```
to:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/mail/attachments?"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("messageId=42"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-1"));
    QVERIFY(!request.contains("sub=sub-1"));
    QVERIFY(!request.contains("hash=hash-1"));
```

Extend `downloadAttachmentReturnsRawBytesAndParsesFilenameFromContentDisposition`'s
assertion block from:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/mail/attachment?"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("messageId=42"));
    QVERIFY(request.contains("index=0"));
```
to:
```cpp
    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/mail/attachment?"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("messageId=42"));
    QVERIFY(request.contains("index=0"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Id: sub-1"));
    QVERIFY(request.contains("X-Kypost-Subscriber-Hash: hash-1"));
    QVERIFY(!request.contains("sub=sub-1"));
    QVERIFY(!request.contains("hash=hash-1"));
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build --target RelayMailSourceTest`
Expected: all 5 touched tests FAIL (headers absent from the still-
unconverted production code); other tests in this file (parsing,
error-mapping, non-auth-focused tests) are unaffected.

- [ ] **Step 3: Rewrite the production code**

In `core/net/RelayMailSource.cpp`, replace `fetchInbox`'s query
construction:
```cpp
    QList<QPair<QString, QString>> query = auth.queryItems();
    if (limit.has_value())
        query.append({ QStringLiteral("limit"), QString::number(*limit) });
    query.append({ QStringLiteral("mailbox"), mailbox });
    if (since.has_value())
        query.append({ QStringLiteral("since"), QString::number(*since) });

    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/inbox")), query);
```
with:
```cpp
    QList<QPair<QString, QString>> query;
    if (limit.has_value())
        query.append({ QStringLiteral("limit"), QString::number(*limit) });
    query.append({ QStringLiteral("mailbox"), mailbox });
    if (since.has_value())
        query.append({ QStringLiteral("since"), QString::number(*since) });

    const HttpClient::HttpResult result = m_httpClient.get(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/inbox")), query, auth.headerItems());
```

Replace `performAction`'s request construction:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/inbox/actions")), auth.queryItems(), body);
```
with:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/inbox/actions")), {}, body, auth.headerItems());
```

Replace `sendMail`'s request construction:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/mail/send")), auth.queryItems(), requestBody);
```
with:
```cpp
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/mail/send")), {}, requestBody, auth.headerItems());
```

Replace `listAttachments`'s query construction:
```cpp
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("mailbox"), mailbox });
    query.append({ QStringLiteral("messageId"), messageId });

    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/mail/attachments")), query);
```
with:
```cpp
    QList<QPair<QString, QString>> query;
    query.append({ QStringLiteral("mailbox"), mailbox });
    query.append({ QStringLiteral("messageId"), messageId });

    const HttpClient::HttpResult result = m_httpClient.get(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/mail/attachments")), query, auth.headerItems());
```

Replace `downloadAttachment`'s query construction:
```cpp
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("mailbox"), mailbox });
    query.append({ QStringLiteral("messageId"), messageId });
    query.append({ QStringLiteral("index"), QString::number(index) });

    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/mail/attachment")), query);
```
with:
```cpp
    QList<QPair<QString, QString>> query;
    query.append({ QStringLiteral("mailbox"), mailbox });
    query.append({ QStringLiteral("messageId"), messageId });
    query.append({ QStringLiteral("index"), QString::number(index) });

    const HttpClient::HttpResult result = m_httpClient.get(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/mail/attachment")), query, auth.headerItems());
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build --target RelayMailSourceTest && ./build/tests/RelayMailSourceTest`
Expected: PASS (all tests)

- [ ] **Step 5: Commit**

```bash
git add core/net/RelayMailSource.cpp tests/core/net/RelayMailSourceTest.cpp
git commit -m "net: send pairing auth as headers in RelayMailSource (all 5 endpoints)"
```

---

### Task 8: Delete `RelayAuth::queryItems()`

By this point every call site in the codebase uses `headerItems()`
instead. `queryItems()` is now dead code.

**Files:**
- Modify: `core/net/RelayAuth.h`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing — this only removes an unused method and tidies the
  doc comment. No call site anywhere in the codebase references
  `queryItems()` after this task (verified in Step 1 below, before making
  the change, and again in Step 2 after).

- [ ] **Step 1: Confirm nothing still calls `queryItems()`**

Run: `grep -rn "queryItems()" --include=*.cpp --include=*.h . | grep -v /build/`
Expected: exactly one match — the definition in `core/net/RelayAuth.h`
itself. If any other match appears, STOP — a call site was missed in
Tasks 2–7 and must be fixed before proceeding with this task.

- [ ] **Step 2: Remove `queryItems()` and tidy the doc comment**

In `core/net/RelayAuth.h`, replace the whole file with:

```cpp
#pragma once

#include <QList>
#include <QPair>
#include <QString>

// Relay auth credentials (subscriber id + hash), sent as
// X-Kypost-Subscriber-Id/X-Kypost-Subscriber-Hash headers on every
// authenticated Relay request. Mirrors llama-Mail-for-Mac's RelayAuth
// (Data/Networking/HTTPClient.swift), read for structure only. Plain value
// type with no store dependency -- callers pull sub/hash out of whatever
// pairing/session store owns them and hand this to HttpClient::get/post.
struct RelayAuth
{
    QString sub;
    QString hash;

    QList<QPair<QString, QString>> headerItems() const
    {
        return { { QStringLiteral("X-Kypost-Subscriber-Id"), sub },
                 { QStringLiteral("X-Kypost-Subscriber-Hash"), hash } };
    }

    bool operator==(const RelayAuth&) const = default;
};
```

- [ ] **Step 3: Run the full build to confirm nothing broke**

Run: `cmake --build build`
Expected: SUCCESS — this is the authoritative proof that no call site
anywhere in the codebase (including any not touched by this plan) still
references the now-deleted `queryItems()`; a stray reference would fail
the build here.

- [ ] **Step 4: Run `RelayAuthTest` to confirm `headerItems()` still works**

Run: `cmake --build build --target RelayAuthTest && ./build/tests/RelayAuthTest`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add core/net/RelayAuth.h
git commit -m "net: delete RelayAuth::queryItems(), now unused after the header migration"
```

---

### Final verification (after all 8 tasks)

- [ ] Run the full test suite: `ctest --test-dir build` — must be fully
  green, per `AGENTS.md`'s own bar ("A change is not verified until this
  builds cleanly and `ctest` is green").
- [ ] `grep -rn '"sub"\|"hash"' core/net/*.cpp | grep -i queryparam` —
  spot-check there's no remaining `addQueryItem`/query-list construction
  anywhere still building a `sub`/`hash` pair (the `grep -rn
  "queryItems()"` check in Task 8 Step 1 is the authoritative one; this is
  a second, independent sanity pass).
- [ ] Manual: run the app against a server running the already-shipped
  header-accepting backend and confirm mail fetch, folder actions, mail
  send, attachment list/download, contact sync (pull/push/dedupe), groups
  fetch, contact photo fetch, and PGP QR token mint all still work
  end-to-end — these endpoints now depend entirely on headers reaching the
  server correctly.

### Out of scope for this plan

- `PgpQrClient::fetchKey`, `NativeRegistrationClient.*`,
  `MfaResponseClient.*` — untouched, as documented in Global Constraints.
- `HttpClient.h`/`HttpClient.cpp` — the `headers` parameter this plan uses
  already exists and is already correct.
- kypost-for-Mac client changes — a separate plan, gated on nothing further
  (the server already accepts headers), but out of scope for this
  Linux-only plan.
- The server-side removal of legacy `?sub=&hash=` query-param support
  (Rollout Step 3 in the server's design doc) — a server-repo change gated
  on client adoption metrics, not part of this plan.
- Fixing the `llama-Mail-for-Mac` vs. `kypost-for-Mac` path mismatch in
  `RelayAuth.h`'s doc comment — pre-existing, unrelated to this migration.
