#include "vcard/VCardContact.h"

#include <QStringList>
#include <optional>

namespace {

constexpr int kFoldLimitOctets = 75; // RFC 6350 sec 3.2 soft line-fold limit

std::optional<QString> emptyToNullopt(const QString& s)
{
    return s.isEmpty() ? std::nullopt : std::optional<QString>(s);
}

bool isUtf8ContinuationByte(char b)
{
    return (static_cast<unsigned char>(b) & 0xC0) == 0x80;
}

// Folds one logical "NAME[;PARAM=...]:value" content line into RFC
// 6350-style CRLF + single-space continuations at 75 octets. Byte- (not
// QChar-) based so a multi-byte UTF-8 character is never split across a
// fold boundary -- back off the cut point while it lands mid-sequence.
QString foldLine(const QString& line)
{
    const QByteArray utf8 = line.toUtf8();
    if (utf8.size() <= kFoldLimitOctets)
        return line;

    QString result;
    int pos = 0;
    bool first = true;
    while (pos < utf8.size()) {
        // A continuation line's leading space itself counts toward its own
        // 75-octet budget, so continuations get one fewer content octet.
        const int budget = first ? kFoldLimitOctets : kFoldLimitOctets - 1;
        int end = qMin(pos + budget, utf8.size());
        while (end > pos && end < utf8.size() && isUtf8ContinuationByte(utf8.at(end)))
            --end;
        if (end <= pos)
            end = pos + 1; // pathological safety net, not expected in practice
        if (!first)
            result += QStringLiteral("\r\n ");
        result += QString::fromUtf8(utf8.mid(pos, end - pos));
        pos = end;
        first = false;
    }
    return result;
}

// Reverses foldLine() across a whole vCard blob: normalizes CRLF/CR/LF line
// endings, then treats any line starting with a space/tab as a continuation
// of the previous logical line (RFC 6350 unfolding), dropping just the one
// leading whitespace character.
QStringList unfoldLines(const QString& vcard)
{
    QString normalized = vcard;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList rawLines = normalized.split(QLatin1Char('\n'));

    QStringList logical;
    for (const QString& raw : rawLines) {
        if (raw.isEmpty())
            continue;
        if ((raw.at(0) == QLatin1Char(' ') || raw.at(0) == QLatin1Char('\t')) && !logical.isEmpty())
            logical.last() += raw.mid(1);
        else
            logical << raw;
    }
    return logical;
}

// TEXT-value escaping shared by every scalar/structured-component property
// (FN, NICKNAME, ORG, TITLE, NOTE, BDAY, REV, and each N/ADR component) --
// RFC 6350 sec 3.4 requires backslash/comma/semicolon/newline escaping
// uniformly across these, not just NOTE (NOTE just happens to be the field
// most likely to contain them in practice).
QString escapeText(const QString& value)
{
    QString out;
    out.reserve(value.size());
    for (const QChar ch : value) {
        if (ch == QLatin1Char('\n')) {
            out += QStringLiteral("\\n");
            continue;
        }
        if (ch == QLatin1Char('\\') || ch == QLatin1Char(',') || ch == QLatin1Char(';'))
            out += QLatin1Char('\\');
        out += ch;
    }
    return out;
}

QString unescapeText(const QString& value)
{
    QString out;
    out.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < value.size()) {
            const QChar next = value.at(i + 1);
            if (next == QLatin1Char('n') || next == QLatin1Char('N')) {
                out += QLatin1Char('\n');
                ++i;
                continue;
            }
            if (next == QLatin1Char(',') || next == QLatin1Char(';') || next == QLatin1Char('\\')) {
                out += next;
                ++i;
                continue;
            }
        }
        out += ch;
    }
    return out;
}

// Splits a still-escaped value on an unescaped delimiter (used for N/ADR's
// ";"-separated components and N's additional-names ","-separated
// sub-values) without disturbing any "\," / "\;" / "\\" sequences -- those
// get resolved later by unescapeText() on each leaf token.
QStringList splitUnescaped(const QString& value, QChar delimiter)
{
    QStringList parts;
    QString current;
    bool escaped = false;
    for (const QChar ch : value) {
        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            current += ch;
            escaped = true;
            continue;
        }
        if (ch == delimiter) {
            parts << current;
            current.clear();
            continue;
        }
        current += ch;
    }
    parts << current;
    return parts;
}

// N's additional-names (3rd) component supports its own "," multi-value
// convention per RFC 6350 (e.g. "Philip,Paul"). Contact.middleName has no
// multi-valued concept, so multiple sub-values are deliberately collapsed
// by joining with a space -- documented lossy-mapping decision, not solved.
QString joinCommaMultiValue(const QString& rawComponent)
{
    const QStringList subParts = splitUnescaped(rawComponent, QLatin1Char(','));
    QStringList unescaped;
    unescaped.reserve(subParts.size());
    for (const QString& part : subParts)
        unescaped << unescapeText(part);
    return unescaped.join(QLatin1Char(' '));
}

void appendTextProperty(QStringList& lines, const QString& name, const std::optional<QString>& value)
{
    if (!value)
        return;
    lines << foldLine(name + QLatin1Char(':') + escapeText(*value));
}

// Wraps a TYPE=/X-LABEL=/LABEL= parameter value in double quotes (the
// RFC 6350 sec 5.1 QSTRING form) whenever it contains a delimiter (';',
// ':', ',') that would otherwise be indistinguishable from content-line
// structure -- e.g. a custom-field label of "a;b" would read back as two
// separate parameters, and a service/label containing ':' would corrupt
// the head/value split entirely. A literal '"' in the source value is
// dropped rather than escaped: vCard 3.0's quoted-string has no escape
// mechanism for an embedded DQUOTE (it is simply excluded from
// QSAFE-CHAR), so there is no lossless way to preserve one -- silently
// dropping it is preferable to corrupting the line or rejecting free-text
// user input outright.
QString quoteParamValueIfNeeded(const QString& rawValue)
{
    QString value = rawValue;
    value.remove(QLatin1Char('"'));
    if (value.contains(QLatin1Char(';')) || value.contains(QLatin1Char(':')) || value.contains(QLatin1Char(',')))
        return QLatin1Char('"') + value + QLatin1Char('"');
    return value;
}

// EMAIL/TEL/ADR share this TYPE= handling: on write, a present label is
// uppercased into a single TYPE= token; PREF is never synthesized here.
// X-RELATED/X-ABDATE reuse the same helper for their (single-value, not a
// real multi-type list) TYPE= param, so the quoting below covers those
// too -- see quoteParamValueIfNeeded's doc comment for why it's needed at
// all now that Task 5 made some of these labels user-typeable.
QString typeParamForWrite(const std::optional<QString>& label)
{
    if (!label || label->isEmpty())
        return QString();
    return QStringLiteral(";TYPE=") + quoteParamValueIfNeeded(label->toUpper());
}

// On read, take the first non-PREF token, lowercased -- multi-type
// (TYPE=HOME,VOICE) collapses to the first token because Contact's
// label field has no secondary-type concept. If every token is PREF (or
// there are none), the label is nullopt rather than inventing one.
std::optional<QString> firstNonPrefTypeLower(const QStringList& tokens)
{
    for (const QString& token : tokens) {
        if (token.isEmpty())
            continue;
        if (token.compare(QStringLiteral("PREF"), Qt::CaseInsensitive) == 0)
            continue;
        return token.toLower();
    }
    return std::nullopt;
}

// URL's non-standard X-LABEL parameter (websites) is free text, not a
// restricted TYPE token set -- unlike typeParamForWrite, case is preserved
// as-is on both write and read rather than upper/lowercased. Still routed
// through quoteParamValueIfNeeded since free text can contain ';'/':'/','.
QString xLabelParamForWrite(const std::optional<QString>& label)
{
    if (!label || label->isEmpty())
        return QString();
    return QStringLiteral(";X-LABEL=") + quoteParamValueIfNeeded(*label);
}

// X-LLAMA-CUSTOM's LABEL parameter is the user-supplied custom-field name
// -- same free-text quoting rule as xLabelParamForWrite, split into its
// own helper since (unlike the others) a custom field always has a LABEL=
// param, never an absent/omitted one.
QString labelParamForWrite(const QString& label)
{
    return QStringLiteral(";LABEL=") + quoteParamValueIfNeeded(label);
}

// Sanitizes an IM service name into the safe token that a vCard property
// name is restricted to (RFC 6350 sec 3.3's name = iana-token / x-name,
// both ALPHA / DIGIT / "-" only): every other character -- including
// spaces, ':', ';' -- is dropped rather than escaped, because unlike a
// parameter value a property name has no quoting mechanism at all. So
// "Google Talk" becomes "GOOGLETALK" and "a:b"/"a;b" can no longer break
// content-line syntax by injecting a colon/semicolon into the property
// name itself. If sanitizing drops every character, the caller falls back
// to the plain (service-less) "X-IMPP" property, matching the existing
// absent-service path rather than emitting a dangling "X-IMPP-".
QString sanitizeImppServiceToken(const QString& service)
{
    QString out;
    out.reserve(service.size());
    for (const QChar ch : service) {
        if (ch.unicode() < 128 && (ch.isLetterOrNumber() || ch == QLatin1Char('-')))
            out += ch.toUpper();
    }
    return out;
}

struct ContentLine
{
    QString name;
    QStringList typeTokens;
    std::optional<QString> xLabelParam; // "X-LABEL=" -- websites/URL only
    std::optional<QString> labelParam; // "LABEL=" -- X-LLAMA-CUSTOM only
    QString value; // still escaped; caller unescapes per-field
};

// Splits one already-unfolded logical line into property name, its TYPE=/
// X-LABEL=/LABEL= parameters (the only parameters this converter needs),
// and raw value. Both the head/value ':' and the inter-parameter ';' are
// found by a quote-aware scan rather than plain indexOf/split: since
// quoteParamValueIfNeeded() (write side) now wraps a param value in
// double quotes whenever it contains ';'/':'/',', a naive first-colon or
// blind semicolon-split would misparse those -- e.g. LABEL="a;b" must not
// be split at the ';' inside the quotes.
ContentLine parseContentLine(const QString& line)
{
    ContentLine result;

    bool inQuotes = false;
    int colonIdx = -1;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"'))
            inQuotes = !inQuotes;
        else if (ch == QLatin1Char(':') && !inQuotes) {
            colonIdx = i;
            break;
        }
    }
    if (colonIdx < 0) {
        result.name = line;
        return result;
    }

    const QString head = line.left(colonIdx);
    result.value = line.mid(colonIdx + 1);

    QStringList headParts;
    {
        QString current;
        bool quoted = false;
        for (const QChar ch : head) {
            if (ch == QLatin1Char('"'))
                quoted = !quoted;
            if (ch == QLatin1Char(';') && !quoted) {
                headParts << current;
                current.clear();
                continue;
            }
            current += ch;
        }
        headParts << current;
    }

    result.name = headParts.isEmpty() ? QString() : headParts.first();
    for (int i = 1; i < headParts.size(); ++i) {
        const QString& param = headParts.at(i);
        const int eq = param.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString paramName = param.left(eq);
        const QString rawParamValue = param.mid(eq + 1);
        const bool wasQuoted = rawParamValue.size() >= 2 && rawParamValue.startsWith(QLatin1Char('"'))
            && rawParamValue.endsWith(QLatin1Char('"'));
        const QString paramValue = wasQuoted ? rawParamValue.mid(1, rawParamValue.size() - 2) : rawParamValue;
        if (paramName.compare(QStringLiteral("TYPE"), Qt::CaseInsensitive) == 0) {
            // A quoted TYPE= value is one atomic (possibly comma-containing)
            // token, not vCard's usual comma-separated multi-type list --
            // quoting is only ever produced by typeParamForWrite() for the
            // single-label X-RELATED/X-ABDATE/etc. case, so splitting it on
            // ',' here would wrongly fragment a label like "a,b".
            result.typeTokens = wasQuoted ? QStringList { paramValue } : paramValue.split(QLatin1Char(','));
        } else if (paramName.compare(QStringLiteral("X-LABEL"), Qt::CaseInsensitive) == 0)
            result.xLabelParam = paramValue;
        else if (paramName.compare(QStringLiteral("LABEL"), Qt::CaseInsensitive) == 0)
            result.labelParam = paramValue;
    }
    return result;
}

ContactEmailEntry parseEmailLine(const ContentLine& cl)
{
    ContactEmailEntry entry;
    entry.label = firstNonPrefTypeLower(cl.typeTokens);
    entry.value = unescapeText(cl.value);
    return entry;
}

ContactPhoneEntry parsePhoneLine(const ContentLine& cl)
{
    ContactPhoneEntry entry;
    entry.label = firstNonPrefTypeLower(cl.typeTokens);
    entry.value = unescapeText(cl.value);
    return entry;
}

ContactAddressEntry parseAddressLine(const ContentLine& cl)
{
    ContactAddressEntry entry;
    entry.label = firstNonPrefTypeLower(cl.typeTokens);
    const QStringList parts = splitUnescaped(cl.value, QLatin1Char(';'));
    auto at = [&](int i) { return i < parts.size() ? parts.at(i) : QString(); };
    // parts[0] = PO box, parts[1] = extended address -- always ignored on
    // read; Contact has no fields for them (see write side for the mirror
    // decision).
    entry.street = emptyToNullopt(unescapeText(at(2)));
    entry.city = emptyToNullopt(unescapeText(at(3)));
    entry.region = emptyToNullopt(unescapeText(at(4)));
    entry.postalCode = emptyToNullopt(unescapeText(at(5)));
    entry.country = emptyToNullopt(unescapeText(at(6)));
    return entry;
}

} // namespace

namespace VCardContact {

QString contactToVCard(const Contact& contact)
{
    QStringList lines;
    lines << QStringLiteral("BEGIN:VCARD");
    lines << QStringLiteral("VERSION:3.0");

    // uid: Contact.uid is NEVER written as vCard UID -- the native item's
    // identity lives entirely in NativeContactLinkDao.native_item_id
    // (Task 2), a separate namespace from Contact.uid (relay identity).
    // rev: relay-only revision counter, no vCard equivalent -- never
    // written.
    // createdAt: no vCard equivalent -- never written (see read side for
    // the nullopt-on-absence mirror).
    // deleted: tombstones are expressed at the provider-adapter level
    // (Task 6+), never encoded in vCard text -- never written here.

    lines << foldLine(QStringLiteral("N:") + escapeText(contact.familyName.value_or(QString())) + QLatin1Char(';')
        + escapeText(contact.givenName.value_or(QString())) + QLatin1Char(';')
        // middleName: single Contact field written as N's 3rd component
        // as-is -- lossy only for genuinely multi-valued middle names,
        // which this app has no way to represent on write (see read side).
        + escapeText(contact.middleName.value_or(QString())) + QLatin1Char(';')
        + escapeText(contact.prefix.value_or(QString())) + QLatin1Char(';')
        + escapeText(contact.suffix.value_or(QString())));

    appendTextProperty(lines, QStringLiteral("FN"), contact.fn);
    appendTextProperty(lines, QStringLiteral("NICKNAME"), contact.nickname);
    // department: ORG's 2nd component (org is the 1st) -- built manually
    // rather than via appendTextProperty (which only handles single-value
    // properties), emitted whenever either component is present.
    if (contact.org || contact.department) {
        lines << foldLine(QStringLiteral("ORG:") + escapeText(contact.org.value_or(QString())) + QLatin1Char(';')
            + escapeText(contact.department.value_or(QString())));
    }
    appendTextProperty(lines, QStringLiteral("TITLE"), contact.title);
    appendTextProperty(lines, QStringLiteral("NOTE"), contact.notes);
    appendTextProperty(lines, QStringLiteral("BDAY"), contact.birthday);
    // updatedAt -> REV: written whenever present; appendTextProperty's
    // no-op-when-absent covers the "absence never means now" half of the
    // decision, the read side covers the other half.
    appendTextProperty(lines, QStringLiteral("REV"), contact.updatedAt);
    appendTextProperty(lines, QStringLiteral("KEY"), contact.pgpKey);
    // phoneticGivenName/phoneticFamilyName -> X-PHONETIC-FIRST-NAME/
    // X-PHONETIC-LAST-NAME: Apple's de facto convention, not a vCard 3.0
    // standard -- unverified against a real KAddressBook/EDS export,
    // re-check when Task 7/8 lands a real backend.
    appendTextProperty(lines, QStringLiteral("X-PHONETIC-FIRST-NAME"), contact.phoneticGivenName);
    appendTextProperty(lines, QStringLiteral("X-PHONETIC-LAST-NAME"), contact.phoneticFamilyName);
    // pronouns: no vCard concept at all (matches Android's own "app-only"
    // treatment) -- never written, and nothing else here depends on it.

    for (const ContactEmailEntry& email : contact.emails) {
        lines << foldLine(QStringLiteral("EMAIL") + typeParamForWrite(email.label) + QLatin1Char(':')
            + escapeText(email.value));
    }
    for (const ContactPhoneEntry& phone : contact.phones) {
        lines << foldLine(
            QStringLiteral("TEL") + typeParamForWrite(phone.label) + QLatin1Char(':') + escapeText(phone.value));
    }
    for (const ContactAddressEntry& addr : contact.addresses) {
        // PO box / extended-address (ADR's first two components) are
        // always empty on write -- Contact has no fields for them; read
        // side ignores whatever a real-world vCard puts there.
        lines << foldLine(QStringLiteral("ADR") + typeParamForWrite(addr.label) + QStringLiteral(":;;")
            + escapeText(addr.street.value_or(QString())) + QLatin1Char(';')
            + escapeText(addr.city.value_or(QString())) + QLatin1Char(';')
            + escapeText(addr.region.value_or(QString())) + QLatin1Char(';')
            + escapeText(addr.postalCode.value_or(QString())) + QLatin1Char(';')
            + escapeText(addr.country.value_or(QString())));
    }

    for (const ContactImEntry& im : contact.ims) {
        // X-IMPP-<SERVICE>: vCard 3.0 has no IMPP property (that's 4.0) --
        // unverified against a real KAddressBook/EDS export, re-check when
        // Task 7/8 lands a real backend. service is user-typeable (Task 5's
        // ContactDetail.qml), so it's run through sanitizeImppServiceToken
        // before being spliced into the property name -- see that helper's
        // doc comment for why dropping unsafe characters (not quoting) is
        // the only option in property-name position.
        const QString sanitizedService = im.service ? sanitizeImppServiceToken(*im.service) : QString();
        const QString serviceSuffix = sanitizedService.isEmpty() ? QString() : QLatin1Char('-') + sanitizedService;
        lines << foldLine(QStringLiteral("X-IMPP") + serviceSuffix + typeParamForWrite(im.label) + QLatin1Char(':')
            + escapeText(im.value));
    }
    for (const ContactUrlEntry& website : contact.websites) {
        // URL;X-LABEL=: vCard 3.0's URL property has no standard label
        // parameter -- unverified against a real KAddressBook/EDS export,
        // re-check when Task 7/8 lands a real backend.
        lines << foldLine(
            QStringLiteral("URL") + xLabelParamForWrite(website.label) + QLatin1Char(':') + escapeText(website.value));
    }
    for (const ContactRelationEntry& relation : contact.relations) {
        // X-RELATED: vCard 3.0 has no RELATED property (that's 4.0) --
        // unverified against a real KAddressBook/EDS export, re-check when
        // Task 7/8 lands a real backend.
        lines << foldLine(QStringLiteral("X-RELATED") + typeParamForWrite(relation.label) + QLatin1Char(':')
            + escapeText(relation.name));
    }
    for (const ContactEventEntry& event : contact.events) {
        // X-ABDATE: non-birthday dates have no vCard 3.0 standard property
        // -- unverified against a real KAddressBook/EDS export, re-check
        // when Task 7/8 lands a real backend. Contact.birthday itself stays
        // the separate BDAY property, unaffected.
        lines << foldLine(
            QStringLiteral("X-ABDATE") + typeParamForWrite(event.label) + QLatin1Char(':') + escapeText(event.date));
    }
    for (const ContactCustomFieldEntry& field : contact.customFields) {
        // X-LLAMA-CUSTOM: app-specific free-form fields have no vCard
        // concept at all -- unverified against a real KAddressBook/EDS
        // export, re-check when Task 7/8 lands a real backend.
        lines << foldLine(QStringLiteral("X-LLAMA-CUSTOM") + labelParamForWrite(field.label) + QLatin1Char(':')
            + escapeText(field.value));
    }

    if (!contact.groupIds.isEmpty()) {
        // groupIDs -> CATEGORIES: a genuine vCard 3.0 standard property, no
        // caveat needed. Display names live server-side (GET /api/groups,
        // Task 2) -- only the membership ids round-trip here.
        QStringList escapedGroupIds;
        escapedGroupIds.reserve(contact.groupIds.size());
        for (const QString& groupId : contact.groupIds)
            escapedGroupIds << escapeText(groupId);
        lines << foldLine(QStringLiteral("CATEGORIES:") + escapedGroupIds.join(QLatin1Char(',')));
    }

    // photoRef -> PHOTO: Contact.photoRef is only an opaque reference
    // string (the actual photo bytes are fetched/cached separately,
    // Task 3), not image data -- so this deliberately writes PHOTO as a
    // plain text value containing photoRef itself, NOT a real
    // base64-encoded ENCODING=b PHOTO property. Writing genuine base64
    // PHOTO data, once photo bytes are available at the vCard-generation
    // call site, is future work.
    appendTextProperty(lines, QStringLiteral("PHOTO"), contact.photoRef);

    lines << QStringLiteral("END:VCARD");
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

Contact contactFromVCard(const QString& vcard)
{
    Contact contact;
    // rev: relay-only concept, never appears in vCard text -- left at
    // Contact{}'s default (0).
    // createdAt: never round-tripped through vCard -- left nullopt so the
    // caller-side merge (ContactSyncRepository.cpp's mergeContact pattern)
    // can fall back to any cached value, per the decision table.
    // uid: Contact.uid stays default-constructed (empty); a parsed UID
    // property is intentionally dropped below, never conflated with it.
    // deleted: no vCard representation -- left at Contact{}'s default
    // (false).

    for (const QString& rawLine : unfoldLines(vcard)) {
        const ContentLine cl = parseContentLine(rawLine);
        const QString name = cl.name.toUpper();

        if (name == QStringLiteral("N")) {
            const QStringList parts = splitUnescaped(cl.value, QLatin1Char(';'));
            auto at = [&](int i) { return i < parts.size() ? parts.at(i) : QString(); };
            contact.familyName = emptyToNullopt(unescapeText(at(0)));
            contact.givenName = emptyToNullopt(unescapeText(at(1)));
            // middleName: N's 3rd (additional-names) component may itself
            // be a ","-separated multi-value list -- joined with a space
            // since Contact.middleName has no multi-valued concept.
            contact.middleName = emptyToNullopt(joinCommaMultiValue(at(2)));
            contact.prefix = emptyToNullopt(unescapeText(at(3)));
            contact.suffix = emptyToNullopt(unescapeText(at(4)));
        } else if (name == QStringLiteral("FN")) {
            contact.fn = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("NICKNAME")) {
            contact.nickname = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("ORG")) {
            // department: ORG's 2nd component (see write side for why this
            // isn't appendTextProperty-shaped).
            const QStringList parts = splitUnescaped(cl.value, QLatin1Char(';'));
            auto at = [&](int i) { return i < parts.size() ? parts.at(i) : QString(); };
            contact.org = emptyToNullopt(unescapeText(at(0)));
            contact.department = emptyToNullopt(unescapeText(at(1)));
        } else if (name == QStringLiteral("TITLE")) {
            contact.title = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("NOTE")) {
            contact.notes = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("BDAY")) {
            contact.birthday = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("REV")) {
            // updatedAt: present -> Some(value); absent -> stays nullopt
            // (Contact{}'s default) -- never defaulted to "now".
            contact.updatedAt = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("EMAIL")) {
            contact.emails.append(parseEmailLine(cl));
        } else if (name == QStringLiteral("TEL")) {
            contact.phones.append(parsePhoneLine(cl));
        } else if (name == QStringLiteral("ADR")) {
            contact.addresses.append(parseAddressLine(cl));
        } else if (name == QStringLiteral("KEY")) {
            contact.pgpKey = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("X-PHONETIC-FIRST-NAME")) {
            contact.phoneticGivenName = emptyToNullopt(unescapeText(cl.value));
        } else if (name == QStringLiteral("X-PHONETIC-LAST-NAME")) {
            contact.phoneticFamilyName = emptyToNullopt(unescapeText(cl.value));
        } else if (name.startsWith(QStringLiteral("X-IMPP"))) {
            // X-IMPP[-<SERVICE>]: mirrors the write side's unverified,
            // vCard-3.0-has-no-IMPP caveat.
            ContactImEntry entry;
            static const QString kImppPrefix = QStringLiteral("X-IMPP");
            if (name.size() > kImppPrefix.size() && name.at(kImppPrefix.size()) == QLatin1Char('-'))
                entry.service = emptyToNullopt(name.mid(kImppPrefix.size() + 1).toLower());
            entry.label = firstNonPrefTypeLower(cl.typeTokens);
            entry.value = unescapeText(cl.value);
            contact.ims.append(entry);
        } else if (name == QStringLiteral("URL")) {
            ContactUrlEntry entry;
            entry.label = cl.xLabelParam ? emptyToNullopt(*cl.xLabelParam) : std::nullopt;
            entry.value = unescapeText(cl.value);
            contact.websites.append(entry);
        } else if (name == QStringLiteral("X-RELATED")) {
            ContactRelationEntry entry;
            entry.label = firstNonPrefTypeLower(cl.typeTokens);
            entry.name = unescapeText(cl.value);
            contact.relations.append(entry);
        } else if (name == QStringLiteral("X-ABDATE")) {
            ContactEventEntry entry;
            entry.label = firstNonPrefTypeLower(cl.typeTokens);
            entry.date = unescapeText(cl.value);
            contact.events.append(entry);
        } else if (name == QStringLiteral("X-LLAMA-CUSTOM")) {
            ContactCustomFieldEntry entry;
            entry.label = cl.labelParam.value_or(QString());
            entry.value = unescapeText(cl.value);
            contact.customFields.append(entry);
        } else if (name == QStringLiteral("CATEGORIES")) {
            // groupIDs: a genuine vCard 3.0 standard property, no caveat
            // needed (see write side).
            for (const QString& rawGroupId : splitUnescaped(cl.value, QLatin1Char(','))) {
                const QString groupId = unescapeText(rawGroupId);
                if (!groupId.isEmpty())
                    contact.groupIds.append(groupId);
            }
        } else if (name == QStringLiteral("PHOTO")) {
            // photoRef: see write side -- this is the opaque reference
            // string, never real base64 photo bytes.
            contact.photoRef = emptyToNullopt(unescapeText(cl.value));
        }
        // BEGIN/VERSION/END/UID and anything else unrecognized: ignored.
        // UID specifically is never conflated with Contact.uid regardless
        // of what a real-world exporter puts there (see decision table).
    }

    return contact;
}

} // namespace VCardContact
