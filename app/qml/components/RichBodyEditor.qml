import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWebEngine
import com.urlxl.mail 1.0

// Compose's rich HTML body editor (supersedes the earlier plain-TextArea-
// only constraint). The editing surface is a contenteditable WebEngineView.
// Unlike EmailDetail.qml's read-only viewer -- which sets
// settings.javascriptEnabled: false because sender HTML is untrusted -- this
// view intentionally enables JavaScript: the script running here is ours
// (execCommand calls + the sanitizer below), a different trust boundary than
// rendering someone else's mail.
Item {
    id: root

    implicitWidth: 360
    implicitHeight: 240

    // Seeds the editor's content. Call exactly once, right after
    // construction (Compose.qml's Component.onCompleted) -- there is no
    // "reload" support, callers only ever seed a fresh draft.
    function loadInitialHtml(html) {
        webView.loadHtml(root.shellHtml(html))
    }

    // Runs the sanitizer over the current DOM and invokes
    // callback({html, isEmpty}). Necessarily asynchronous: runJavaScript
    // crosses into WebEngine's separate render process, there is no
    // synchronous variant.
    function requestSendableHtml(callback) {
        webView.runJavaScript(root.sanitizerScript, callback)
    }

    function shellHtml(bodyHtml) {
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
            + "body { font-family: sans-serif; font-size: 14px; margin: 8px; "
            + "background: " + Theme.panel + "; color: " + Theme.inkStrong + "; }"
            + "</style></head><body contenteditable=\"true\">" + bodyHtml + "</body></html>"
    }

    // Installed after every loadHtml() (see WebEngineView.onLoadingChanged
    // below) -- forces paste to plain text so arbitrary markup/scripts from
    // a webpage never enter the DOM in the first place. The sanitizer below
    // is a backstop, not the primary control.
    readonly property string pasteScript: "
        document.body.addEventListener('paste', function(event) {
            event.preventDefault();
            var text = (event.clipboardData || window.clipboardData).getData('text/plain');
            document.execCommand('insertText', false, text);
        });
    "

    // Allowlist DOM sanitizer: unwraps (keeps children, drops the tag) any
    // element outside the fixed tag list, strips every attribute except
    // href/style on <a> (further restricted below). Runs against a cloned
    // subtree so it never mutates what's on screen.
    readonly property string sanitizerScript: "
        (function() {
            var allowedTags = ['P', 'BR', 'DIV', 'B', 'STRONG', 'I', 'EM', 'U', 'A', 'BLOCKQUOTE'];
            var allowedStyleProps = ['color', 'background-color', 'padding', 'border-radius',
                                      'display', 'font-weight', 'text-decoration', 'border'];
            function isSafeUrl(url) {
                return /^(https?:|mailto:)/i.test(url);
            }
            function clean(node) {
                Array.from(node.childNodes).forEach(function(child) {
                    if (child.nodeType === Node.ELEMENT_NODE) {
                        clean(child);
                        if (allowedTags.indexOf(child.tagName) === -1) {
                            while (child.firstChild) node.insertBefore(child.firstChild, child);
                            node.removeChild(child);
                            return;
                        }
                        Array.from(child.attributes).forEach(function(attr) {
                            if (child.tagName === 'A' && attr.name === 'href') {
                                if (!isSafeUrl(child.getAttribute('href'))) child.removeAttribute('href');
                            } else if (child.tagName === 'A' && attr.name === 'style') {
                                var kept = attr.value.split(';').map(function(rule) {
                                    var prop = (rule.split(':')[0] || '').trim().toLowerCase();
                                    return allowedStyleProps.indexOf(prop) !== -1 ? rule.trim() : null;
                                }).filter(Boolean).join('; ');
                                if (kept) child.setAttribute('style', kept); else child.removeAttribute('style');
                            } else {
                                child.removeAttribute(attr.name);
                            }
                        });
                    } else if (child.nodeType !== Node.TEXT_NODE) {
                        node.removeChild(child);
                    }
                });
            }
            var clone = document.body.cloneNode(true);
            clean(clone);
            return { html: clone.innerHTML, isEmpty: document.body.textContent.trim() === '' };
        })();
    "

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            IconButton {
                icon: "format-text-bold"
                tooltip: i18n("Bold")
                onClicked: webView.runJavaScript("document.execCommand('bold')")
            }
            IconButton {
                icon: "format-text-italic"
                tooltip: i18n("Italic")
                onClicked: webView.runJavaScript("document.execCommand('italic')")
            }
            IconButton {
                icon: "format-text-underline"
                tooltip: i18n("Underline")
                onClicked: webView.runJavaScript("document.execCommand('underline')")
            }
            IconButton {
                icon: "insert-link"
                tooltip: i18n("Insert Link")
                onClicked: linkDialog.open()
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.shapeField
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            WebEngineView {
                id: webView
                anchors.fill: parent
                anchors.margins: 1
                backgroundColor: Theme.panel
                settings.javascriptEnabled: true

                onLoadingChanged: function(loadRequest) {
                    if (loadRequest.status === WebEngineView.LoadSucceededStatus)
                        webView.runJavaScript(root.pasteScript)
                }
            }
        }
    }

    HyperlinkDialog {
        id: linkDialog
        z: 10
        anchors.fill: parent
        onLinkConfirmed: function(label, url, asButton) {
            const style = asButton
                ? " style=\"display:inline-block;padding:10px 20px;border-radius:" + Theme.shapeButton
                    + "px;background-color:" + Theme.accent + ";color:" + Theme.readableOnAccent
                    + ";text-decoration:none;font-weight:600;\""
                : ""
            const escapedLabel = label.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
            const escapedUrl = url.replace(/&/g, "&amp;").replace(/"/g, "&quot;")
            const html = "<a href=\"" + escapedUrl + "\"" + style + ">" + escapedLabel + "</a>"
            webView.runJavaScript("document.execCommand('insertHTML', false, " + JSON.stringify(html) + ")")
        }
    }
}
