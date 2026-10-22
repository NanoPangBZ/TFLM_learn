(function () {
    var PRISM_THEMES = {
        dark: "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism-tomorrow.min.css",
        light: "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism.min.css"
    };

    function applyTheme(theme) {
        document.documentElement.setAttribute("data-theme", theme);
        var prismLink = document.getElementById("prism-theme");
        if (prismLink) prismLink.href = PRISM_THEMES[theme];
        document.querySelectorAll(".theme-btn").forEach(function (btn) {
            btn.classList.toggle("is-active", btn.getAttribute("data-theme") === theme);
        });
        try { localStorage.setItem("protocol-doc-theme", theme); } catch (e) {}
        highlightCodeBlocks();
    }

    function highlightCodeBlocks() {
        if (!window.Prism) return;
        document.querySelectorAll("code.language-c").forEach(function (el) {
            if (el.closest(".code-linked")) return;
            Prism.highlightElement(el);
        });
    }

    var initial = document.documentElement.getAttribute("data-theme") || "dark";
    applyTheme(initial);

    document.querySelectorAll(".theme-btn").forEach(function (btn) {
        btn.addEventListener("click", function () {
            applyTheme(btn.getAttribute("data-theme"));
        });
    });


    // 代码复制
    function copyToClipboard(text, btn, okLabel, defaultLabel) {
        function onOk() {
            btn.textContent = okLabel;
            btn.classList.add("is-copied");
            setTimeout(function () {
                btn.textContent = defaultLabel;
                btn.classList.remove("is-copied");
            }, 1500);
        }
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).then(onOk).catch(function () {
                fallbackCopy(text, onOk);
            });
        } else {
            fallbackCopy(text, onOk);
        }
    }
    document.querySelectorAll(".code-copy").forEach(function (btn) {
        btn.addEventListener("click", function () {
            var wrap = btn.closest(".code-block-wrap");
            var code = wrap ? wrap.querySelector("code") : null;
            if (!code) return;
            copyToClipboard(code.textContent, btn, "已复制", "复制");
        });
    });
    document.querySelectorAll(".code-copy-name").forEach(function (btn) {
        btn.addEventListener("click", function () {
            var name = btn.getAttribute("data-struct-name");
            if (!name) return;
            copyToClipboard(name, btn, "已复制", "复制名");
        });
    });
    function fallbackCopy(text, cb) {
        var ta = document.createElement("textarea");
        ta.value = text;
        ta.style.cssText = "position:fixed;opacity:0";
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand("copy"); cb(); } catch (e) {}
        document.body.removeChild(ta);
    }

    // 侧栏搜索 + 正文过滤
    var searchInput = document.getElementById("toc-search");
    var searchHint = document.getElementById("toc-search-hint");
    var tocGroups = document.querySelectorAll(".toc-group");
    var tocLeaves = document.querySelectorAll(".toc-leaf");
    var msgCards = document.querySelectorAll(".msg-card");
    var typeCards = document.querySelectorAll(".type-card");
    var modules = document.querySelectorAll(".module");

    function norm(s) { return (s || "").toLowerCase().trim(); }

    function applySearch() {
        var q = norm(searchInput.value);
        var matchCount = 0;

        tocLeaves.forEach(function (leaf) {
            var hit = !q || (leaf.getAttribute("data-search") || "").indexOf(q) !== -1;
            leaf.classList.toggle("is-hidden", !hit);
            var link = leaf.querySelector("a");
            if (!link) return;
            var idEl = link.querySelector(".toc-id");
            var titleEl = link.querySelector(".toc-title");
            if (!q) {
                if (idEl) idEl.textContent = leaf.getAttribute("data-toc-id") || idEl.textContent;
                if (titleEl) titleEl.textContent = leaf.getAttribute("data-toc-title") || titleEl.textContent;
                return;
            }
            if (idEl) highlightText(idEl, q);
            if (titleEl) highlightText(titleEl, q);
        });

        tocGroups.forEach(function (group) {
            var visible = group.querySelectorAll(".toc-leaf:not(.is-hidden)").length;
            var groupHit = !q || (group.getAttribute("data-search") || "").indexOf(q) !== -1;
            group.querySelectorAll(".toc-subgroup").forEach(function (subgroup) {
                var subVisible = subgroup.querySelectorAll(".toc-leaf:not(.is-hidden)").length;
                subgroup.classList.toggle("is-hidden", subVisible === 0 && q);
                if (subVisible > 0 && q) {
                    var details = subgroup.querySelector("details");
                    if (details) details.open = true;
                }
            });
            group.classList.toggle("is-hidden", visible === 0 && !groupHit);
            if ((groupHit || visible > 0) && q) {
                var details = group.querySelector(":scope > .toc-details");
                if (details) details.open = true;
            }
        });

        msgCards.forEach(function (card) {
            var hit = !q || (card.getAttribute("data-search") || "").indexOf(q) !== -1;
            card.classList.toggle("is-hidden", !hit);
            if (hit) matchCount++;
        });

        typeCards.forEach(function (card) {
            var hit = !q || (card.getAttribute("data-search") || "").indexOf(q) !== -1;
            card.classList.toggle("is-hidden", !hit);
            if (hit) matchCount++;
        });

        modules.forEach(function (mod) {
            var visibleMsg = mod.querySelectorAll(".msg-card:not(.is-hidden)").length;
            var visibleType = mod.querySelectorAll(".type-card:not(.is-hidden)").length;
            mod.style.display = visibleMsg === 0 && visibleType === 0 && q ? "none" : "";
        });

        if (!q) {
            searchHint.textContent = "";
        } else {
            searchHint.textContent = "匹配 " + matchCount + " 条";
        }
    }

    function highlightText(el, q) {
        var raw = el.textContent;
        var idx = raw.toLowerCase().indexOf(q);
        if (idx === -1) { el.textContent = raw; return; }
        var before = raw.slice(0, idx);
        var mid = raw.slice(idx, idx + q.length);
        var after = raw.slice(idx + q.length);
        el.innerHTML = escapeHtml(before) + "<mark>" + escapeHtml(mid) + "</mark>" + escapeHtml(after);
    }
    function escapeHtml(s) {
        return s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;");
    }

    var searchTimer;
    searchInput.addEventListener("input", function () {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(applySearch, 120);
    });

    // 跳转后卡片高亮
    var flashTimer = null;
    function flashCards(cards) {
        document.querySelectorAll(".msg-card.is-flash, .type-card.is-flash").forEach(function (c) {
            c.classList.remove("is-flash");
        });
        cards.forEach(function (c) {
            c.classList.remove("is-flash");
            void c.offsetWidth;
            c.classList.add("is-flash");
        });
        clearTimeout(flashTimer);
        flashTimer = setTimeout(function () {
            cards.forEach(function (c) { c.classList.remove("is-flash"); });
        }, 2000);
    }

    var jumpReturnStack = [];
    var activeBackJumpEl = null;
    var activeBackJumpCard = null;

    function removeBackJumpButton() {
        if (activeBackJumpEl && activeBackJumpEl.parentNode) {
            activeBackJumpEl.parentNode.removeChild(activeBackJumpEl);
        }
        activeBackJumpEl = null;
        activeBackJumpCard = null;
    }

    function removeBackJump() {
        removeBackJumpButton();
        jumpReturnStack = [];
    }

    function returnFromJump() {
        if (!jumpReturnStack.length) return;
        removeBackJumpButton();
        var state = jumpReturnStack.pop();
        window.scrollTo({ top: state.scrollY, behavior: "smooth" });
        if (state.hash) {
            history.replaceState(null, "", state.hash);
            var origin = document.querySelector(state.hash);
            if (origin) {
                flashCards([origin]);
                pinActiveToc(origin.id, 1200);
                if (jumpReturnStack.length) {
                    attachBackJumpToCard(origin);
                }
            }
        } else {
            history.replaceState(null, "", window.location.pathname + window.location.search);
            updateActiveToc();
        }
    }

    function attachBackJumpToCard(card) {
        removeBackJumpButton();
        if (!jumpReturnStack.length || !card) return;
        var depth = jumpReturnStack.length;
        var btn = document.createElement("button");
        btn.type = "button";
        btn.className = "card-back-jump";
        btn.title = depth > 1
            ? ("返回上一级（还可返回 " + (depth - 1) + " 级）")
            : "返回跳转前位置";
        btn.setAttribute("aria-label", btn.title);
        btn.innerHTML = (
            '<span class="card-back-jump-icon" aria-hidden="true">↩</span>'
            + '<span class="card-back-jump-text">返回'
            + (depth > 1 ? (' <span class="card-back-jump-depth">' + depth + "</span>") : "")
            + "</span>"
        );
        btn.addEventListener("click", function (e) {
            e.preventDefault();
            e.stopPropagation();
            returnFromJump();
        });
        card.appendChild(btn);
        activeBackJumpEl = btn;
        activeBackJumpCard = card;
    }

    function hideTypePreview() {
        var previewEl = document.getElementById("type-preview");
        if (!previewEl) return;
        if (typePreviewHideTimer) {
            clearTimeout(typePreviewHideTimer);
            typePreviewHideTimer = null;
        }
        previewEl.classList.remove("is-visible");
        previewEl.setAttribute("aria-hidden", "true");
        var inner = previewEl.querySelector(".type-preview-inner");
        if (inner) inner.innerHTML = "";
        typePreviewAnchor = "";
    }

    function navigateToCard(target, options) {
        if (!target) return;
        hideTypePreview();
        options = options || {};
        var enableReturn = options.enableReturn === true;
        if (enableReturn) {
            jumpReturnStack.push({
                scrollY: window.scrollY,
                hash: window.location.hash || ""
            });
        } else {
            removeBackJump();
        }
        target.scrollIntoView({ behavior: "smooth", block: "start" });
        history.replaceState(null, "", "#" + target.id);
        flashCards([target]);
        pinActiveToc(target.id, 1200);
        if (enableReturn) {
            attachBackJumpToCard(target);
        }
    }

    document.querySelectorAll(".msg-card, .type-card").forEach(function (card) {
        card.addEventListener("click", function (e) {
            if (!activeBackJumpCard || card === activeBackJumpCard) return;
            if (e.target.closest(".type-ref, .jump-btn, .card-back-jump, .code-copy, .code-copy-name")) return;
            removeBackJump();
        });
    });

    // 滚动时高亮当前目录项
    var tocLinks = document.querySelectorAll(".toc-sub a");
    var pinnedActiveId = "";
    var pinTimer = null;
    var scrollRaf = 0;

    function setActiveTocByCardId(cardId) {
        if (!cardId) return;
        tocLinks.forEach(function (a) {
            a.classList.toggle("is-active", a.getAttribute("href") === "#" + cardId);
        });
    }

    function pinActiveToc(cardId, ms) {
        pinnedActiveId = cardId;
        setActiveTocByCardId(cardId);
        clearTimeout(pinTimer);
        pinTimer = setTimeout(function () {
            pinnedActiveId = "";
            updateActiveToc();
        }, ms || 1000);
    }

    function isNearBottom() {
        return window.innerHeight + window.scrollY >= document.documentElement.scrollHeight - 100;
    }

    function visibleCards() {
        return Array.prototype.filter.call(
            document.querySelectorAll(".msg-card, .type-card"),
            function (c) { return !c.classList.contains("is-hidden"); }
        );
    }

    function updateActiveToc() {
        if (pinnedActiveId) {
            setActiveTocByCardId(pinnedActiveId);
            return;
        }
        var cards = visibleCards();
        if (!cards.length) return;

        if (isNearBottom()) {
            setActiveTocByCardId(cards[cards.length - 1].id);
            return;
        }

        var viewTop = window.scrollY + window.innerHeight * 0.22;
        var best = null;
        var bestDist = Infinity;
        cards.forEach(function (card) {
            var rect = card.getBoundingClientRect();
            var cardTop = rect.top + window.scrollY;
            if (rect.bottom < window.innerHeight * 0.12) return;
            if (rect.top > window.innerHeight * 0.88) return;
            var dist = Math.abs(cardTop - viewTop);
            if (dist < bestDist) {
                bestDist = dist;
                best = card;
            }
        });
        if (best) setActiveTocByCardId(best.id);
    }

    var backTop = document.getElementById("back-top");
    window.addEventListener("scroll", function () {
        cancelAnimationFrame(scrollRaf);
        scrollRaf = requestAnimationFrame(updateActiveToc);
        backTop.classList.toggle("is-visible", window.scrollY > 400);
    }, { passive: true });
    updateActiveToc();

    backTop.addEventListener("click", function () {
        window.scrollTo({ top: 0, behavior: "smooth" });
    });

    // 二级目录：跳转并高亮对应卡片
    tocLinks.forEach(function (a) {
        a.addEventListener("click", function (e) {
            var id = a.getAttribute("href");
            if (!id || id.charAt(0) !== "#") return;
            var target = document.querySelector(id);
            if (!target) return;
            e.preventDefault();
            navigateToCard(target, { enableReturn: false });
        });
    });

    // 代码块内类型引用 / 卡片跳转按钮
    document.querySelectorAll(".type-ref, .jump-btn").forEach(function (a) {
        a.addEventListener("click", function (e) {
            e.preventDefault();
            e.stopPropagation();
            var id = a.getAttribute("href");
            if (!id || id.charAt(0) !== "#") return;
            var target = document.querySelector(id);
            if (!target) return;
            navigateToCard(target, { enableReturn: true });
        });
    });

    // 消息卡片跳转按钮悬停：鼠标右侧浮现类型预览
    var typePreviewEl = document.getElementById("type-preview");
    var typePreviewInner = typePreviewEl ? typePreviewEl.querySelector(".type-preview-inner") : null;
    var typePreviewHead = typePreviewEl ? typePreviewEl.querySelector(".type-preview-head") : null;
    var typePreviewAnchor = "";
    var typePreviewHideTimer = null;

    function cancelTypePreviewHide() {
        if (typePreviewHideTimer) {
            clearTimeout(typePreviewHideTimer);
            typePreviewHideTimer = null;
        }
    }

    function scheduleTypePreviewHide() {
        cancelTypePreviewHide();
        typePreviewHideTimer = setTimeout(hideTypePreview, 320);
    }

    function isPreviewHoverTarget(el) {
        if (!el || !el.closest) return false;
        return !!el.closest(".msg-card .jump-btn, #type-preview");
    }

    function findPreviewScrollables(start, root) {
        var chain = [];
        var el = start;
        while (el && el !== root && root.contains(el)) {
            if (el.scrollHeight > el.clientHeight + 1) {
                var oy = window.getComputedStyle(el).overflowY;
                if (oy === "auto" || oy === "scroll" || oy === "overlay") {
                    chain.push(el);
                }
            }
            el = el.parentElement;
        }
        return chain;
    }

    function canPreviewScroll(el, deltaY) {
        if (deltaY < 0) return el.scrollTop > 0;
        if (deltaY > 0) {
            return el.scrollTop + el.clientHeight < el.scrollHeight - 1;
        }
        return false;
    }

    function trapPreviewWheel(e) {
        if (!typePreviewEl || !typePreviewEl.contains(e.target)) return;
        var chain = findPreviewScrollables(e.target, typePreviewEl);
        for (var i = 0; i < chain.length; i++) {
            if (canPreviewScroll(chain[i], e.deltaY)) return;
        }
        e.preventDefault();
    }

    function positionTypePreview(btn, clientX, clientY) {
        if (!typePreviewEl) return;
        var pad = 10;
        var gap = 14;
        var hostCard = btn ? btn.closest(".msg-card") : null;
        if (hostCard) {
            typePreviewEl.style.width = hostCard.getBoundingClientRect().width + "px";
        }
        typePreviewEl.style.left = (clientX + gap) + "px";
        typePreviewEl.style.top = (clientY - 6) + "px";
        var rect = typePreviewEl.getBoundingClientRect();
        var left = clientX + gap;
        var top = clientY - 6;
        if (left + rect.width > window.innerWidth - pad) {
            left = clientX - rect.width - gap;
        }
        if (top + rect.height > window.innerHeight - pad) {
            top = window.innerHeight - rect.height - pad;
        }
        if (top < pad) top = pad;
        if (left < pad) left = pad;
        typePreviewEl.style.left = left + "px";
        typePreviewEl.style.top = top + "px";
    }

    function buildTypePreviewCard(sourceCard) {
        var clone = sourceCard.cloneNode(true);
        clone.removeAttribute("id");
        clone.querySelectorAll("[id]").forEach(function (el) { el.removeAttribute("id"); });
        clone.querySelectorAll(".card-back-jump, .card-jumps").forEach(function (el) { el.remove(); });
        clone.classList.add("type-preview-card");
        return clone;
    }

    function showTypePreview(anchor, btn, clientX, clientY) {
        if (!typePreviewEl || !typePreviewInner || !anchor) return;
        cancelTypePreviewHide();
        var contentChanged = anchor !== typePreviewAnchor;
        var shouldPosition = contentChanged || !typePreviewEl.classList.contains("is-visible");
        if (contentChanged) {
            var source = document.getElementById(anchor);
            if (!source) return;
            typePreviewInner.innerHTML = "";
            typePreviewInner.appendChild(buildTypePreviewCard(source));
            typePreviewAnchor = anchor;
            if (typePreviewHead) {
                var nameEl = source.querySelector(".type-name");
                typePreviewHead.textContent = nameEl ? nameEl.textContent.trim() : "类型预览";
            }
        }
        typePreviewEl.classList.add("is-visible");
        typePreviewEl.setAttribute("aria-hidden", "false");
        if (shouldPosition) {
            positionTypePreview(btn, clientX, clientY);
        }
    }

    if (typePreviewEl && typePreviewInner) {
        document.querySelectorAll(".msg-card .jump-btn").forEach(function (btn) {
            btn.addEventListener("mouseenter", function (e) {
                var anchor = btn.getAttribute("data-jump-target")
                    || (btn.getAttribute("href") || "").replace(/^#/, "");
                showTypePreview(anchor, btn, e.clientX, e.clientY);
            });
            btn.addEventListener("mouseleave", function (e) {
                if (isPreviewHoverTarget(e.relatedTarget)) return;
                scheduleTypePreviewHide();
            });
        });
        typePreviewEl.addEventListener("mouseenter", cancelTypePreviewHide);
        typePreviewEl.addEventListener("mouseleave", function (e) {
            if (isPreviewHoverTarget(e.relatedTarget)) return;
            scheduleTypePreviewHide();
        });
        typePreviewEl.addEventListener("wheel", trapPreviewWheel, { passive: false });
        window.addEventListener("scroll", hideTypePreview, { passive: true });
    }
})();
