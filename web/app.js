"use strict";

const view = document.getElementById("view");
const modal = document.getElementById("desk-dialog");
const dialogBody = document.getElementById("dialog-body");
const titles = {
  overview: "Overview", catalog: "Catalog", readers: "Readers",
  circulation: "Circulation", reservations: "Reservations", reports: "Reports",
};
const desk = {
  data: null, searchTerm: "", searchMatches: null, searchError: "",
  onlyAvailable: false, readerTerm: "", loanFilter: "active", queueBook: null,
};
const dateFormatter = new Intl.DateTimeFormat("en-GB", {
  day: "numeric", month: "short", year: "numeric", timeZone: "UTC",
});
const charMap = {"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"};
const e = value => String(value).replace(/[&<>"']/g, char => charMap[char]);
const d = value => dateFormatter.format(new Date(value + "T12:00:00Z"));
const icon = name => `<svg class="icon" aria-hidden="true"><use href="#i-${name}"></use></svg>`;
const plural = (n, singular, multiple = singular + "s") => n + " " + (n === 1 ? singular : multiple);
const idOrder = (a,b) => a === b ? 0 : a < b ? -1 : 1;
const tier = value => value === "FACULTY" ? "Faculty" : "Student";
const page = () => Object.hasOwn(titles, location.hash.slice(1)) ? location.hash.slice(1) : "overview";
const tag = (label, style = "") => `<span class="tag ${style}">${e(label)}</span>`;
const book = id => desk.data.books.find(item => item.id === id);
const reader = id => desk.data.readers.find(item => item.id === id);
const bookName = id => book(id)?.title ?? id;
const readerName = id => reader(id)?.name ?? id;
const button = (label, action, attrs = {}, style = "", image = "") => {
  const data = Object.entries(attrs).map(([key, value]) => ` data-${key}="${e(value)}"`).join("");
  return `<button type="button" class="button ${style}" data-do="${action}"${data}>${image ? icon(image) : ""}${e(label)}</button>`;
};
const empty = message => `<p class="empty">${e(message)}</p>`;
const heading = (title, subtitle, actions = "") => `<div class="page-heading">
  <div><h1>${title}</h1><p>${subtitle}</p></div>${actions ? `<div class="actions">${actions}</div>` : ""}
</div>`;

async function api(path, options = {}) {
  let response;
  try {
    response = await fetch(path, { ...options, headers: { Accept: "application/json", ...options.headers } });
  } catch {
    throw new Error("Could not reach StackDesk. Check that the local desk is running.");
  }
  let body;
  try {
    body = await response.json();
  } catch {
    throw new Error("The desk returned an unreadable response.");
  }
  if (!response.ok) throw new Error(body.error || "The desk could not complete the request.");
  return body;
}

let toastTimer;
function flash(message, error = false) {
  const feedback = document.getElementById("feedback");
  clearTimeout(toastTimer);
  feedback.textContent = message;
  feedback.classList.toggle("error", error);
  feedback.hidden = false;
  toastTimer = setTimeout(() => { feedback.hidden = true; }, 6000);
}

function counts() {
  return desk.data.books.reduce((total, row) => ({
    copies: total.copies + row.copies, available: total.available + row.available,
    borrowed: total.borrowed + row.borrowed, ready: total.ready + row.ready,
    waiting: total.waiting + row.holds.length - row.ready,
  }), { copies: 0, available: 0, borrowed: 0, ready: 0, waiting: 0 });
}

function stockCards() {
  const c = counts();
  const overdue = desk.data.loans.filter(loan => loan.daysLate > 0).length;
  return `<section class="metrics" aria-label="Library totals">
    ${[
      ["Catalog titles", desk.data.books.length, plural(c.copies, "copy", "copies") + " in the collection", "book", ""],
      ["Available copies", c.available, "On the shelf now", "stock", ""],
      ["Active loans", c.borrowed, overdue ? plural(overdue, "overdue loan") : "No overdue loans", "loan", "blue"],
      ["Ready for pickup", c.ready, plural(c.waiting, "reader") + " on waitlists", "clock", "wait"],
    ].map(([label,n,note,mark,style]) => `<div class="metric">
      <div class="metric-head"><span>${label}</span><span class="metric-icon ${style}">${icon(mark)}</span></div>
      <strong>${n}</strong><small>${note}</small>
    </div>`).join("")}
  </section>`;
}

function copyTrack(row) {
  const pieces = [["free",row.available],["loan",row.borrowed],["held",row.ready]];
  return `<span class="copy-track" aria-hidden="true">${pieces.filter(p => p[1] > 0)
    .map(([key,n]) => `<span class="fill-${key}" style="flex:${n}"></span>`).join("")}</span>`;
}

function availability(row) {
  if (row.available) return tag("Available", "good");
  if (row.ready) return tag("Held for pickup", "wait");
  return tag("All copies out", "blue");
}

// Search stays in C++. This view only hides unavailable titles when the checkbox is selected.
function catalogPanel(short) {
  return `<section class="panel book-catalog" data-short="${short}">
    <div class="panel-head"><div><h2>${short ? "Catalog at a glance" : "Book records"}</h2>
      <p>Smart Search finds titles, authors, IDs and topics, even with common typos.</p></div>
      ${short ? '<a class="button small" href="#catalog">View catalog</a>' : ""}
    </div>
    <form class="search-bar" id="catalog-search" role="search">
      <div class="search-field">${icon("search")}<label for="search-term" class="sr-only">Smart Search</label>
        <input id="search-term" name="term" type="search" placeholder="Search by title, author, ID or topic" maxlength="160" value="${e(desk.searchTerm)}" autocomplete="off">
      </div>
      <label class="checkbox"><input id="available-filter" type="checkbox" ${desk.onlyAvailable ? "checked" : ""}> Available only</label>
    </form>
    <p id="catalog-note" class="filter-note" role="status"></p>
    <div id="book-results"></div>
  </section>`;
}

function drawCatalog() {
  const outlet = document.getElementById("book-results");
  if (!outlet) return;
  const short = outlet.closest("[data-short]").dataset.short === "true";
  const q = desk.searchTerm.trim();
  const note = document.getElementById("catalog-note");
  if (desk.searchError) {
    note.textContent = desk.searchError;
    outlet.innerHTML = empty("Search is temporarily unavailable.");
    return;
  }
  if (q && !desk.searchMatches) {
    note.textContent = "Searching the catalog…";
    outlet.innerHTML = "";
    return;
  }
  const matches = q ? desk.searchMatches.map(match => ({ row: book(match.id), fuzzy: match.fuzzy })).filter(m => m.row) :
    desk.data.books.map(row => ({ row, fuzzy: false }));
  const shown = matches.filter(item => !desk.onlyAvailable || item.row.available);
  const display = short && !q ? shown.slice(0,5) : shown;
  note.textContent = q ? plural(shown.length, "match", "matches") + " for “" + q + "”" : "";
  if (!shown.length) {
    outlet.innerHTML = empty(q || desk.onlyAvailable ? "No books match your filters." : "Add a book to start your catalog.");
    return;
  }
  outlet.innerHTML = `<div class="table-scroll"><table><caption class="sr-only">Book catalog</caption>
    <thead><tr><th scope="col">Title</th><th scope="col" class="${short ? "overview-stock" : "secondary-column"}">Copies</th><th scope="col">Status</th></tr></thead>
    <tbody>${display.map(({row,fuzzy}) => `<tr>
      <td><button class="table-link" data-do="book" data-book="${e(row.id)}" type="button">
        <span class="cell-title">${e(row.title)}</span><span class="cell-sub">${e(row.id)} · ${e(row.author)}${fuzzy ? " · Close match" : ""}</span>
      </button></td>
      <td class="${short ? "overview-stock" : "secondary-column"} mono-number">${row.available} / ${row.copies}${copyTrack(row)}</td>
      <td>${availability(row)}</td>
    </tr>`).join("")}
    </tbody>
  </table></div>`;
}

let searchTimer;
let searchSequence = 0;
function scheduleSearch(immediate = false) {
  clearTimeout(searchTimer);
  const sequence = ++searchSequence;
  const term = desk.searchTerm.trim();
  desk.searchMatches = null;
  desk.searchError = "";
  drawCatalog();
  if (!term) return;
  searchTimer = setTimeout(async () => {
    try {
      const matches = await api("/api/search?q=" + encodeURIComponent(term));
      if (sequence !== searchSequence) return;
      desk.searchMatches = matches;
    } catch (error) {
      if (sequence !== searchSequence) return;
      desk.searchError = error.message;
    }
    drawCatalog();
  }, immediate ? 0 : 160);
}

function queueChoice() {
  if (book(desk.queueBook)) return desk.queueBook;
  const selected = desk.data.books.find(b => b.holds.length)?.id ?? desk.data.books[0]?.id ?? null;
  desk.queueBook = selected;
  return selected;
}

function pickupPanel(full) {
  if (!queueChoice()) return `<section class="panel"><div class="panel-head"><h2>Pickup queue</h2></div>${empty("Add a book to view its queue.")}</section>`;
  return `<section class="panel" aria-label="Pickup queue" data-full-queue="${full}">
    <div class="panel-head"><div><h2>Pickup queue</h2><p>Reservations in the order they were placed.</p></div></div>
    <div class="pickup-book"><label for="queue-book">Book</label><select id="queue-book">
      ${desk.data.books.map(b => `<option value="${e(b.id)}"${b.id === desk.queueBook ? " selected" : ""}>${e(b.title)}</option>`).join("")}
    </select></div>
    <div id="queue-list"></div>
    ${full ? "" : '<div class="panel-footer"><a class="text-button" href="#reservations">Manage reservations</a></div>'}
  </section>`;
}

function drawQueue() {
  const outlet = document.getElementById("queue-list");
  if (!outlet) return;
  const current = book(desk.queueBook);
  const full = outlet.closest("[data-full-queue]").dataset.fullQueue === "true";
  if (!current.holds.length) {
    outlet.innerHTML = empty("Nobody is waiting for this book.");
    return;
  }
  outlet.innerHTML = `<p class="pickup-summary" role="status">${plural(current.ready,"ready pickup")} · ${current.holds.length-current.ready} waiting</p>
    <ol class="queue-list" aria-label="Reservations for ${e(current.title)}">
      ${current.holds.map((hold,index) => {
        const member = reader(hold.reader);
        const fields = { book: current.id, reader: hold.reader };
        return `<li class="queue-item">
          <span class="queue-position" aria-hidden="true">${index+1}</span>
          <div><span class="cell-title">${e(member.name)}</span><span class="cell-sub">${e(tier(member.tier))} · ${hold.readyThrough ? "Collect by " + d(hold.readyThrough) : "Waiting for a copy"}</span></div>
          ${full ? `<div class="queue-actions">${hold.readyThrough ? button("Collect", "borrow", fields, "small primary") : ""}
            ${button("Cancel hold", "cancel-hold", fields, "small")}</div>` : hold.readyThrough ? tag("Ready","wait") : tag("Waiting")}
        </li>`;
      }).join("")}
    </ol>`;
}

function policiesPanel() {
  return `<section class="panel"><div class="panel-head"><h2>Loan policies</h2></div>
    <div class="policy-list">${desk.data.policies.map(p => `<div class="policy-row">
      <div><b>${tier(p.tier)}</b><p>Up to ${plural(p.maxLoans,"title")} · ${plural(p.maxRenewals,"renewal")}</p></div>
      ${tag(plural(p.days,"day"),p.tier === "FACULTY" ? "blue" : "good")}
    </div>`).join("")}</div>
  </section>`;
}

function overview() {
  const actions = button("Add book","add-book",{},"","add") + button("New loan","borrow",{},"primary","add");
  return heading("Library overview","Check stock, manage loans and see who is next in line.",actions) + stockCards() +
    `<div class="dashboard-grid">${catalogPanel(true)}<div class="stack">${pickupPanel(false)}${policiesPanel()}</div></div>` +
    (desk.data.demo ? `<aside class="demo-note">${icon("clock")}<p><strong>Try the pickup workflow.</strong> Return Practical C++ in Circulation, then advance the date by three days and check the queue.</p></aside>` : "");
}

function readersPage() {
  return heading("Readers","Register members and review their borrowing history.",button("Register reader","add-reader",{},"primary","add")) +
    `<section class="panel"><div class="panel-head"><h2>Reader directory</h2><span class="cell-sub">${plural(desk.data.readers.length,"reader")}</span></div>
      <div class="search-bar"><div class="search-field">${icon("search")}<label for="reader-search" class="sr-only">Search readers</label>
        <input id="reader-search" type="search" autocomplete="off" value="${e(desk.readerTerm)}" placeholder="Search name or reader ID"></div></div>
      <div id="reader-results"></div></section>`;
}

function drawReaders() {
  const outlet = document.getElementById("reader-results");
  if (!outlet) return;
  const q = desk.readerTerm.trim().toLowerCase();
  const members = desk.data.readers.filter(m => [m.id,m.name].some(v => v.toLowerCase().includes(q)));
  if (!members.length) {
    outlet.innerHTML = empty(q ? "No readers found." : "Register a reader to start lending.");
    return;
  }
  outlet.innerHTML = `<div class="table-scroll"><table><caption class="sr-only">Reader directory</caption>
    <thead><tr><th scope="col">Reader</th><th scope="col">Tier</th><th scope="col">Active loans</th><th scope="col">Actions</th></tr></thead>
    <tbody>${members.map(m => {
      const active = desk.data.loans.filter(l => l.reader === m.id && !l.returned).length;
      return `<tr><td><button class="table-link" type="button" data-do="reader" data-reader="${e(m.id)}">
        <span class="cell-title">${e(m.name)}</span><span class="cell-sub">${e(m.id)} · ${e(m.contact)}</span></button></td>
        <td>${tag(tier(m.tier),m.tier === "FACULTY" ? "blue" : "good")}</td>
        <td class="mono-number">${active}</td><td class="action-cell">${button("Edit","edit-reader",{reader:m.id},"small")}</td></tr>`;
    }).join("")}</tbody></table></div>`;
}

function filteredLoans() {
  return desk.data.loans.filter(loan =>
    desk.loanFilter === "overdue" ? loan.daysLate > 0 :
      desk.loanFilter === "returned" ? Boolean(loan.returned) : !loan.returned);
}

function drawLoans() {
  const outlet = document.getElementById("loan-results");
  if (!outlet) return;
  const loans = filteredLoans();
  document.querySelectorAll("[data-loans]").forEach(btn => {
    const active = btn.dataset.loans === desk.loanFilter;
    btn.classList.toggle("active",active);
    btn.setAttribute("aria-pressed",String(active));
  });
  if (!loans.length) {
    outlet.innerHTML = empty("No " + desk.loanFilter + " loans.");
    return;
  }
  outlet.innerHTML = `<div class="table-scroll"><table><caption class="sr-only">Loans</caption>
    <thead><tr><th scope="col">Book and reader</th><th scope="col">Due date</th><th scope="col">Status</th><th scope="col">Actions</th></tr></thead>
    <tbody>${loans.map(loan => {
      const args = {book:loan.book, reader:loan.reader};
      const state = loan.returned ? tag("Returned","good") : loan.daysLate ? tag(plural(loan.daysLate,"day") + " overdue","bad") : tag("On loan","blue");
      return `<tr><td><span class="cell-title">${e(bookName(loan.book))}</span><span class="cell-sub">${e(readerName(loan.reader))} · ${tier(loan.tier)}</span></td>
        <td class="mono-number">${d(loan.due)}${loan.returned ? `<span class="cell-sub">Returned ${d(loan.returned)}</span>` : ""}</td>
        <td>${state}</td><td class="action-cell">${loan.returned ? "" : `<div class="actions">
        ${button("Renew","renew",args,"small")}${button("Return","return",args,"small")}</div>`}</td></tr>`;
    }).join("")}</tbody></table></div>`;
}

function circulationPage() {
  return heading("Circulation","Check out, return and renew books.",button("New loan","borrow",{},"primary","add")) +
    `<section class="panel"><div class="panel-head"><h2>Loan register</h2></div>
      <div class="toolbar" aria-label="Loan status">
        ${[["active","Active"],["overdue","Overdue"],["returned","Returned"]].map(([id,label]) => `<button class="chip" type="button" data-loans="${id}" aria-pressed="false">${label}</button>`).join("")}
      </div><div id="loan-results"></div>
    </section>`;
}

function reportsPage() {
  const late = desk.data.loans.filter(l => l.daysLate > 0).sort((a,b) =>
    b.daysLate - a.daysLate || idOrder(a.book,b.book) || idOrder(a.reader,b.reader));
  const usage = new Map();
  desk.data.loans.forEach(l => { usage.set(l.book,(usage.get(l.book) ?? 0) + 1); });
  const top = [...usage].sort((a,b) => b[1]-a[1] || idOrder(a[0],b[0])).slice(0,3);
  return heading("Reports","Review overdue items and see which books are borrowed most.",
    `<a class="button primary" href="/api/overdue.csv" download>${icon("download")} Download overdue CSV</a>`) +
    stockCards() + `<div class="dashboard-grid">
      <section class="panel"><div class="panel-head"><div><h2>Overdue loans</h2><p>Oldest due dates first.</p></div></div>
      ${late.length ? `<div class="table-scroll"><table><caption class="sr-only">Overdue items</caption>
        <thead><tr><th scope="col">Book</th><th scope="col">Reader</th><th scope="col">Due date</th><th scope="col">Days late</th></tr></thead>
        <tbody>${late.map(l => `<tr><td>${e(bookName(l.book))}</td><td>${e(readerName(l.reader))}</td><td class="mono-number">${d(l.due)}</td><td>${tag(l.daysLate,"bad")}</td></tr>`).join("")}</tbody></table></div>` : empty("There are no overdue books.")}
      </section>
      <section class="panel"><div class="panel-head"><h2>Most borrowed titles</h2></div>
      ${top.length ? `<ol class="queue-list">${top.map(([id,n],i) => `<li class="queue-item">
        <span class="queue-position" aria-hidden="true">${i+1}</span><span class="cell-title">${e(bookName(id))}</span>${tag(plural(n,"loan"),"good")}</li>`).join("")}</ol>` : empty("Checkouts will appear here.")}</section>
    </div>`;
}

function render() {
  if (!desk.data) return;
  const current = page();
  document.getElementById("location-name").textContent = titles[current];
  document.getElementById("session-mode").textContent = desk.data.demo ? "Demo session" : "Local desk";
  const dateLabel = document.getElementById("business-date");
  dateLabel.dateTime = desk.data.day;
  dateLabel.textContent = d(desk.data.day);
  document.getElementById("advance-date").hidden = !desk.data.demo;
  document.querySelectorAll(".navigation [data-page]").forEach(link => {
    if (link.dataset.page === current) link.setAttribute("aria-current","page");
    else link.removeAttribute("aria-current");
  });
  const surfaces = {
    overview, readers: readersPage, circulation: circulationPage, reports: reportsPage,
    catalog: () => heading("Catalog","Find books and manage titles and copy counts.",button("Add book","add-book",{},"primary","add")) + catalogPanel(false),
    reservations: () => heading("Reservations","Reserve checked-out books and handle pickups.",
      button("New reservation","reserve",{},"primary","add")) + `<div class="dashboard-grid">${pickupPanel(true)}${policiesPanel()}</div>`,
  };
  view.innerHTML = surfaces[current]();
  view.setAttribute("aria-busy","false");
  drawCatalog();
  drawQueue();
  drawReaders();
  drawLoans();
}

const textField = (key, label, value = "", attrs = "") => `<label>${label}
  <input data-field name="${key}" required value="${e(value)}" ${attrs}></label>`;
const menuField = (key, label, records, value = "") => `<label>${label}
  <select data-field name="${key}" required>
    <option value="">Choose a ${label.toLowerCase()}</option>
    ${records.map(([id,name]) => `<option value="${e(id)}"${value === id ? " selected" : ""}>${e(name)}</option>`).join("")}
  </select></label>`;

function showDialog(title, html) {
  document.getElementById("dialog-title").textContent = title;
  dialogBody.innerHTML = html;
  if (!modal.open) modal.showModal();
  (dialogBody.querySelector("input:not([readonly]), select") || document.getElementById("dialog-close")).focus();
}

function form(title, action, content, submitLabel = "Save changes") {
  showDialog(title, `<form class="desk-form" data-command="${action}">
    ${content}<p class="form-error" role="alert" hidden></p>
    <div class="actions"><button class="button primary" type="submit">${submitLabel}</button>
      <button class="button" data-do="close" type="button">Cancel</button></div>
  </form>`);
}

function bookForm(action, id) {
  const b = book(id);
  const base = 'maxlength="24" pattern="[A-Za-z0-9_\\-]+"';
  form(b ? "Edit book" : "Add book", action,
    textField("id","Book ID",b?.id,base + (b ? " readonly" : ' placeholder="e.g. BK901"')) +
    textField("title","Title",b?.title,'maxlength="160"') +
    textField("author","Author",b?.author,'maxlength="160"') +
    textField("copies","Total copies",b?.copies ?? 1,'type="number" min="1" max="9999" step="1"'),
    b ? "Save changes" : "Add book");
}

function readerForm(action, id) {
  const m = reader(id);
  form(m ? "Edit reader" : "Register reader", action,
    textField("id","Reader ID",m?.id,'maxlength="24" pattern="[A-Za-z0-9_\\-]+"' + (m ? " readonly" : ' placeholder="e.g. R123"')) +
    textField("name","Name",m?.name,'maxlength="160"') +
    textField("contact","Contact",m?.contact,'maxlength="160" placeholder="Email or phone"') +
    menuField("tier","Tier",[["STUDENT","Student"],["FACULTY","Faculty"]],m?.tier ?? "STUDENT"),
    m ? "Save changes" : "Register reader");
}

function transactionForm(action, data) {
  const borrowing = action === "borrow";
  const labels = [borrowing ? "Check out book" : "Place reservation", borrowing ? "Check out" : "Reserve book"];
  const books = desk.data.books.map(b => [b.id, b.title + " (" + b.id + ")"]);
  const readers = desk.data.readers.map(r => [r.id, r.name + " (" + r.id + ")"]);
  if (!books.length || !readers.length) {
    showDialog(labels[0], "<p>Add a book and register a reader before starting a loan or reservation.</p>");
    return;
  }
  form(labels[0], action,
    menuField("book","Book",books,data.book) + menuField("reader","Reader",readers,data.reader), labels[1]);
}

function confirmAction(action, data) {
  const label = {"return":"Return book", "renew":"Renew loan", "cancel-hold":"Cancel hold"}[action];
  form(label, action, `<p>${e(bookName(data.book))} · ${e(readerName(data.reader))}</p>
    <input data-field type="hidden" value="${e(data.book)}"><input data-field type="hidden" value="${e(data.reader)}">`, label);
}

function topicsForm(id) {
  const b = book(id);
  form("Edit topics","set-topics",
    `<p>Topics help readers find alternatives for ${e(b.title)}.</p>
    <input data-field type="hidden" value="${e(id)}">
    <label>Topics, separated by commas
      <input data-field name="topics" value="${e(b.topics.join(","))}" maxlength="160"
        placeholder="e.g. programming, c++, beginner">
    </label>`,
    "Save topics");
}

function suggestionReason(item) {
  const reasons = [];
  if (item.topics.length) reasons.push("Topics: " + item.topics.join(", "));
  if (item.similarReaders) reasons.push("Also borrowed by readers with similar histories");
  return reasons.join(" · ");
}

function suggestionList(picks, readerId = null) {
  return `<ul class="suggestion-list">
    ${picks.map(item => {
      const args = readerId ? {book:item.id,reader:readerId} : {book:item.id};
      return `<li class="suggestion">
        <div><span class="cell-title">${e(item.title)}</span>
          <span class="cell-sub">${e(item.author)} · ${plural(item.available,"available copy","available copies")}</span>
          <span class="cell-sub">${e(suggestionReason(item))}</span>
        </div>
        ${button("Check out","borrow",args,"small")}
      </li>`;
    }).join("")}
  </ul>`;
}

async function showSuggestions(kind, id, outletId, display) {
  const outlet = document.getElementById(outletId);
  try {
    const picks = await api("/api/recommendations?" + kind + "=" + encodeURIComponent(id));
    if (modal.open && outlet.isConnected) outlet.innerHTML = display(picks);
  } catch (error) {
    if (modal.open && outlet.isConnected)
      outlet.innerHTML = `<p class="form-error" role="alert">${e(error.message)}</p>`;
  }
}

function inspectBook(id) {
  const b = book(id);
  const primary = b.available
    ? button("Check out","borrow",{book:id},"primary")
    : button("Join waitlist","reserve",{book:id},"primary");
  const actions = primary + button("Edit book","edit-book",{book:id}) + button("Edit topics","set-topics",{book:id});
  const topics = b.topics.length ? `<div class="topic-list" aria-label="Topics">${b.topics.map(t => tag(t)).join("")}</div>` : "";
  showDialog(b.title, `<p>${e(b.id)} · ${e(b.author)}</p>
    ${topics}
    <div class="detail-data"><div><b>${b.available}</b><small>Available</small></div>
    <div><b>${b.borrowed}</b><small>On loan</small></div><div><b>${b.ready}</b><small>Ready for pickup</small></div></div>
    <p>${b.holds.length ? plural(b.holds.length,"reservation") + " in the pickup queue." : "No current reservations."}</p>
    <div class="actions">${actions}</div>
    ${b.available ? "" : `<section class="suggestion-section" aria-label="Available alternatives">
      <h3>Available alternatives</h3><div id="book-picks" aria-live="polite"><p>Finding books with shared topics…</p></div>
    </section>`}`);
  if (!b.available) showSuggestions("book",id,"book-picks",picks =>
    picks.length ? suggestionList(picks) : "<p>No available books share this title's topics.</p>");
}

function inspectReader(id) {
  const m = reader(id);
  const history = desk.data.loans.filter(l => l.reader === id).reverse();
  showDialog(m.name, `<p>${e(m.id)} · ${tier(m.tier)} · ${e(m.contact)}</p>
    <div class="actions">${button("Edit reader","edit-reader",{reader:id})}${button("New loan","borrow",{reader:id},"primary")}</div>
    <section class="suggestion-section" aria-label="Recommended for this reader">
      <h3>Recommended for ${e(m.name)}</h3>
      <div id="reader-picks" aria-live="polite"><p>Finding available recommendations…</p></div>
    </section>
    <div class="actions"><h3>Loan history</h3></div>
    ${history.length ? `<ul class="history-list">${history.map(l =>
      `<li><span class="cell-title">${e(bookName(l.book))}</span><span class="cell-sub">${l.returned ? "Returned " + d(l.returned) : "Due " + d(l.due)}</span></li>`).join("")}</ul>` :
      "<p>No recorded loans.</p>"}`);
  showSuggestions("reader",id,"reader-picks",picks => {
    if (!picks.length) return "<p>No available recommendations from this reader's history.</p>";
    return suggestionList(picks,id) + `<div class="actions">${button("Preview email draft","draft",{reader:id})}</div>`;
  });
}

function emailDraft(id) {
  const m = reader(id);
  showDialog("Recommendation email draft",
    '<div id="email-draft" aria-live="polite"><p>Preparing available titles…</p></div>');
  showSuggestions("reader",id,"email-draft",picks => {
    if (!picks.length) return "<p>There are no available recommendations to include.</p>";
    const subject = "Your StackDesk reading recommendations";
    const message = [
      "Hi " + m.name + ",", "",
      "Here are a few books you might like that are currently available at the library:", "",
      ...picks.map(item => "- " + item.title + " by " + item.author + " (" + item.id + ")"),
      "", "Availability can change. Please check with the library desk before visiting.", "", "StackDesk Library",
    ].join("\n");
    const address = m.contact.trim();
    const hasEmail = /^[a-z0-9._+-]+@[a-z0-9.-]+\.[a-z]{2,}$/i.test(address);
    const [mailbox,domain] = hasEmail ? address.split("@") : ["",""];
    const compose = "mailto:" + encodeURIComponent(mailbox) + "@" + domain +
      "?subject=" + encodeURIComponent(subject) + "&body=" + encodeURIComponent(message.replace(/\n/g,"\r\n"));
    return `<dl class="draft-info">
      <dt>To</dt><dd>${hasEmail ? e(address) : "No email on file"}</dd>
      <dt>Subject</dt><dd>${e(subject)}</dd>
    </dl>
    <label class="draft-body">Message<textarea id="recommendation-message" rows="10" readonly>${e(message)}</textarea></label>
    <div class="actions">
      ${button("Copy message","copy-draft")}
      ${hasEmail && !desk.data.demo ? `<a class="button primary" href="${e(compose)}">Open in mail app</a>` : ""}
      ${button("Back to reader","reader",{reader:id})}
    </div>
    <p id="draft-status" class="draft-status" role="status">${desk.data.demo ? "Demo contact. Use Copy message to try the draft." :
      hasEmail ? "Review the draft before opening it in your mail app." : "Add an email address to open this draft in a mail app."}</p>`;
  });
}

async function copyDraft() {
  const text = document.getElementById("recommendation-message");
  const status = document.getElementById("draft-status");
  try {
    await navigator.clipboard.writeText(text.value);
    status.textContent = "Message copied.";
  } catch {
    text.focus();
    text.select();
    status.textContent = "Press Control+C or Command+C to copy the selected message.";
  }
}

function nextDate(day, count) {
  const value = new Date(day + "T12:00:00Z");
  value.setUTCDate(value.getUTCDate() + count);
  return value.toISOString().slice(0,10);
}

function actionClick(data) {
  switch (data.do) {
    case "close": modal.close(); break;
    case "book": inspectBook(data.book); break;
    case "set-topics": topicsForm(data.book); break;
    case "draft": emailDraft(data.reader); break;
    case "copy-draft": copyDraft(); break;
    case "reader": inspectReader(data.reader); break;
    case "add-book": case "edit-book": bookForm(data.do,data.book); break;
    case "add-reader": case "edit-reader": readerForm(data.do,data.reader); break;
    case "borrow": case "reserve": transactionForm(data.do,data); break;
    case "return": case "renew": case "cancel-hold": confirmAction(data.do,data); break;
    case "advance":
      if (!desk.data.demo) break;
      form("Advance demo date","advance",
        textField("date","New business date",nextDate(desk.data.day,3),'type="date" min="' + e(desk.data.day) + '"'),
        "Advance date");
      break;
  }
}

document.addEventListener("click", event => {
  const action = event.target.closest("[data-do]");
  if (action && desk.data) actionClick(action.dataset);
  const loanTab = event.target.closest("[data-loans]");
  if (loanTab) {
    desk.loanFilter = loanTab.dataset.loans;
    drawLoans();
  }
});

document.addEventListener("input", event => {
  if (event.target.id === "search-term") {
    desk.searchTerm = event.target.value;
    scheduleSearch();
  } else if (event.target.id === "reader-search") {
    desk.readerTerm = event.target.value;
    drawReaders();
  }
});

document.addEventListener("change", event => {
  if (event.target.id === "available-filter") {
    desk.onlyAvailable = event.target.checked;
    drawCatalog();
  } else if (event.target.id === "queue-book") {
    desk.queueBook = event.target.value;
    drawQueue();
  }
});

let saving = false;
document.addEventListener("submit", async event => {
  const target = event.target;
  if (target.id === "catalog-search") {
    event.preventDefault();
    scheduleSearch(true);
    return;
  }
  if (!target.dataset.command) return;
  event.preventDefault();
  if (saving) return;
  saving = true;
  const submit = target.querySelector('button[type="submit"]');
  const error = target.querySelector(".form-error");
  submit.disabled = true;
  error.hidden = true;
  try {
    const args = [...target.querySelectorAll("[data-field]")].map(field => field.value.trim());
    const result = await api("/api/actions", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({action:target.dataset.command,args}),
    });
    desk.data = result.state;
    modal.close();
    render();
    if (desk.searchTerm.trim()) scheduleSearch(true);
    if (target.dataset.command === "set-topics") inspectBook(args[0]);
    flash(result.message);
  } catch (problem) {
    error.textContent = problem.message;
    error.hidden = false;
    if (!modal.open) flash(problem.message,true);
  } finally {
    saving = false;
    submit.disabled = false;
  }
});

document.getElementById("dialog-close").addEventListener("click", () => modal.close());
window.addEventListener("hashchange", () => {
  render();
  window.scrollTo(0,0);
});

(async function connect() {
  try {
    desk.data = await api("/api/state");
    render();
  } catch (error) {
    view.setAttribute("aria-busy","false");
    view.innerHTML = `<div class="page-heading"><h1>StackDesk is unavailable</h1></div>
      <section class="panel empty" role="alert"><p>${e(error.message)}</p></section>`;
  }
})();
