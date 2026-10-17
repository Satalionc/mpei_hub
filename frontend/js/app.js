/* МЭИ.Hub */
const API = '';
const LS_GROUP = 'mpei_hub_group';
const LS_TOKEN = 'mpei_hub_token';
const LS_THEME = 'mpei_hub_theme';

const WEEKDAYS = ['Понедельник','Вторник','Среда','Четверг','Пятница','Суббота','Воскресенье'];
const WEEKDAYS_SHORT = ['Пн','Вт','Ср','Чт','Пт','Сб','Вс'];

// ===================== Theme =====================
const btnTheme = document.getElementById('btn-theme');
const iconTheme = document.getElementById('icon-theme');

const ICON_SUN = '<path stroke-linecap="round" stroke-linejoin="round" d="M12 3v1.5M12 19.5V21M4.5 12H3m18 0h-1.5M6.3 6.3l-1-1m13.4 13.4-1-1M6.3 17.7l-1 1m13.4-13.4-1 1M12 8a4 4 0 100 8 4 4 0 000-8z"/>';
const ICON_MOON = '<path stroke-linecap="round" stroke-linejoin="round" d="M21 12.8A9 9 0 1111.2 3a7 7 0 009.8 9.8z"/>';
const ICON_SYSTEM = '<path stroke-linecap="round" stroke-linejoin="round" d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z"/>';

function getThemePref() { return localStorage.getItem(LS_THEME) || 'system'; }
function isDark(pref) {
  if (pref === 'dark') return true;
  if (pref === 'light') return false;
  return window.matchMedia('(prefers-color-scheme: dark)').matches;
}
function applyTheme(pref) {
  document.documentElement.classList.toggle('dark', isDark(pref));
  if (pref === 'system') {
    iconTheme.innerHTML = ICON_SYSTEM;
    btnTheme.title = 'Тема: как в системе';
  } else if (isDark(pref)) {
    iconTheme.innerHTML = ICON_SUN;
    btnTheme.title = 'Тема: тёмная';
  } else {
    iconTheme.innerHTML = ICON_MOON;
    btnTheme.title = 'Тема: светлая';
  }
}
function cycleTheme() {
  const order = ['system', 'light', 'dark'];
  const cur = getThemePref();
  const next = order[(Math.max(0, order.indexOf(cur)) + 1) % order.length];
  localStorage.setItem(LS_THEME, next);
  applyTheme(next);
}
applyTheme(getThemePref());
btnTheme.addEventListener('click', e => { e.preventDefault(); cycleTheme(); });
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => {
  if (getThemePref() === 'system') applyTheme('system');
});

// ===================== Tabs =====================
const tabSchedule = document.getElementById('tab-schedule');
const tabGrades = document.getElementById('tab-grades');
const tabMap = document.getElementById('tab-map');
const panelSchedule = document.getElementById('panel-schedule');
const panelGrades = document.getElementById('panel-grades');
const panelMap = document.getElementById('panel-map');

function setTabActive(btn, on) {
  btn.classList.toggle('text-hub-600', on);
  btn.classList.toggle('dark:text-hub-400', on);
  btn.classList.toggle('bg-hub-50', on);
  btn.classList.toggle('dark:bg-hub-900/40', on);
  btn.classList.toggle('text-slate-500', !on);
  btn.classList.toggle('dark:text-slate-400', !on);
}

function activateTab(name) {
  setTabActive(tabSchedule, name === 'schedule');
  setTabActive(tabGrades, name === 'grades');
  setTabActive(tabMap, name === 'map');
  panelSchedule.classList.toggle('hidden', name !== 'schedule');
  panelGrades.classList.toggle('hidden', name !== 'grades');
  panelMap.classList.toggle('hidden', name !== 'map');
}
tabSchedule.addEventListener('click', () => activateTab('schedule'));
tabGrades.addEventListener('click', () => activateTab('grades'));
tabMap.addEventListener('click', () => activateTab('map'));

// ===================== Time helpers (Moscow) =====================
function moscowNow() {
  // "HH:MM" and Date parts in Europe/Moscow
  const fmt = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'Europe/Moscow',
    year: 'numeric', month: '2-digit', day: '2-digit',
    hour: '2-digit', minute: '2-digit', hour12: false,
  });
  const parts = Object.fromEntries(fmt.formatToParts(new Date()).map(p => [p.type, p.value]));
  return {
    dateIso: `${parts.year}-${parts.month}-${parts.day}`,
    minutes: parseInt(parts.hour, 10) * 60 + parseInt(parts.minute, 10),
  };
}

function parseHM(hm) {
  if (!hm) return null;
  const [h, m] = hm.split(':').map(Number);
  if (Number.isNaN(h) || Number.isNaN(m)) return null;
  return h * 60 + m;
}

function formatMinutes(min) {
  if (min < 60) return `${min} мин`;
  const h = Math.floor(min / 60);
  const m = min % 60;
  if (m === 0) return h === 1 ? '1 ч' : `${h} ч`;
  return `${h} ч ${m} мин`;
}

function isLessonPast(dayIso, endTime) {
  const now = moscowNow();
  if (dayIso < now.dateIso) return true;
  if (dayIso > now.dateIso) return false;
  const end = parseHM(endTime);
  if (end == null) return false;
  return now.minutes >= end;
}

// ===================== Schedule =====================
const groupInput = document.getElementById('group-input');
const btnLoad = document.getElementById('btn-load-schedule');
const scheduleContainer = document.getElementById('schedule-container');
const scheduleMeta = document.getElementById('schedule-meta');
const dayNav = document.getElementById('day-nav');
const dayTitle = document.getElementById('day-title');
const daySubtitleText = document.getElementById('day-subtitle-text');
const btnPrev = document.getElementById('btn-prev-day');
const btnNext = document.getElementById('btn-next-day');
const btnWeekPick = document.getElementById('btn-week-pick');
const weekPicker = document.getElementById('week-picker');
const weekList = document.getElementById('week-list');

const savedGroup = localStorage.getItem(LS_GROUP);
if (savedGroup) groupInput.value = savedGroup;

let weekData = null;
let activeIndex = 0;
let weekMonday = null;
let albumBuilt = false;

const KIND_BORDER = {
  'Лекция': 'border-l-blue-500',
  'Практическое': 'border-l-emerald-500',
  'Практика': 'border-l-emerald-500',
  'Лабораторн': 'border-l-amber-500',
  'Семинар': 'border-l-violet-500',
};
const KIND_BADGE = {
  'Лекция': 'bg-blue-100 text-blue-700 dark:bg-blue-900/40 dark:text-blue-300',
  'Практическое': 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/40 dark:text-emerald-300',
  'Практика': 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/40 dark:text-emerald-300',
  'Лабораторн': 'bg-amber-100 text-amber-700 dark:bg-amber-900/40 dark:text-amber-300',
  'Семинар': 'bg-violet-100 text-violet-700 dark:bg-violet-900/40 dark:text-violet-300',
};

function matchKind(kind, map, fallback) {
  if (!kind) return fallback;
  for (const [k, v] of Object.entries(map)) if (kind.includes(k)) return v;
  return fallback;
}
function escapeHtml(s) {
  return String(s ?? '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function mondayOf(d) {
  const x = new Date(d); x.setHours(12,0,0,0);
  const day = x.getDay();
  x.setDate(x.getDate() + (day === 0 ? -6 : 1 - day));
  return x;
}
function toISO(d) {
  return `${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}-${String(d.getDate()).padStart(2,'0')}`;
}
function addDays(d, n) { const x = new Date(d); x.setDate(x.getDate()+n); return x; }

function weekDatesFromMonday(mondayIso) {
  const start = new Date(mondayIso + 'T12:00:00');
  const todayIso = moscowNow().dateIso;
  const out = [];
  for (let i = 0; i < 7; i++) {
    const d = addDays(start, i);
    const iso = toISO(d);
    out.push({
      iso, weekday: WEEKDAYS[i], short: WEEKDAYS_SHORT[i],
      dayNum: d.getDate(),
      month: d.toLocaleDateString('ru-RU', { month: 'long' }),
      isToday: iso === todayIso,
      lessons: [],
    });
  }
  return out;
}

function buildWeekOptions() {
  const thisMon = mondayOf(new Date());
  const opts = [];
  for (let w = -4; w <= 4; w++) {
    const mon = addDays(thisMon, w * 7);
    const sun = addDays(mon, 6);
    const monIso = toISO(mon);
    let label = w === 0 ? 'Текущая неделя' : w === -1 ? 'Прошлая неделя' : w === 1 ? 'Следующая неделя'
      : w < 0 ? `${-w} нед. назад` : `Через ${w} нед.`;
    const range = `${mon.toLocaleDateString('ru-RU',{day:'numeric',month:'short'})} – ${sun.toLocaleDateString('ru-RU',{day:'numeric',month:'short'})}`;
    opts.push({ monIso, label, range });
  }
  return opts;
}

function renderLesson(L, dayIso) {
  const room = [L.auditorium, L.building].filter(Boolean).join(' · ');
  const border = matchKind(L.kind_of_work, KIND_BORDER, 'border-l-slate-300 dark:border-l-slate-600');
  const badge = matchKind(L.kind_of_work, KIND_BADGE, 'bg-slate-100 text-slate-600 dark:bg-slate-800 dark:text-slate-300');
  const past = isLessonPast(dayIso, L.end_time);
  const pastCls = past ? 'lesson-past' : '';
  return `
    <article class="lesson-card border-l-4 ${border} ${pastCls} rounded-r-xl rounded-l-md p-3
                    bg-slate-50 dark:bg-slate-800/60 border border-slate-100 dark:border-slate-700/50">
      <div class="flex items-start justify-between gap-2 mb-1">
        <time class="text-sm font-semibold tabular-nums">${escapeHtml(L.begin_time)} – ${escapeHtml(L.end_time)}</time>
        ${room ? `<span class="text-[11px] text-slate-500 dark:text-slate-400 text-right leading-tight max-w-[42%]">${escapeHtml(room)}</span>` : ''}
      </div>
      <h4 class="font-semibold text-[14px] leading-snug mb-1">${escapeHtml(L.discipline)}</h4>
      <div class="flex flex-wrap items-center gap-1.5 mt-1">
        ${L.kind_of_work ? `<span class="text-[10px] font-medium px-1.5 py-0.5 rounded-md ${badge}">${escapeHtml(L.kind_of_work)}</span>` : ''}
        ${L.lecturer ? `<span class="text-[11px] text-slate-500 dark:text-slate-400">${escapeHtml(L.lecturer)}</span>` : ''}
      </div>
    </article>`;
}

function renderBreak(prevEnd, nextBegin) {
  const a = parseHM(prevEnd);
  const b = parseHM(nextBegin);
  if (a == null || b == null || b <= a) return '';
  const mins = b - a;
  if (mins <= 0) return '';
  return `
    <div class="break-chip text-[11px] text-slate-400 dark:text-slate-500 select-none">
      <span>перерыв · ${formatMinutes(mins)}</span>
    </div>`;
}

function renderDayBody(day) {
  if (!day.lessons.length) {
    return `<div class="flex items-center justify-center min-h-[200px] text-slate-400 dark:text-slate-500 text-sm
                border border-dashed border-slate-200 dark:border-slate-700 rounded-xl bg-slate-50/50 dark:bg-slate-800/30">
              нет пар
            </div>`;
  }
  const parts = [];
  day.lessons.forEach((L, idx) => {
    if (idx > 0) {
      parts.push(renderBreak(day.lessons[idx - 1].end_time, L.begin_time));
    }
    parts.push(renderLesson(L, day.iso));
  });
  return `<div class="space-y-1">${parts.join('')}</div>`;
}

function updateDayHeader() {
  if (!weekData) return;
  const d = weekData.days[activeIndex];
  dayTitle.textContent = d.weekday + (d.isToday ? ' · сегодня' : '');
  daySubtitleText.textContent = `${d.dayNum} ${d.month}` +
    (d.lessons.length ? ` · ${d.lessons.length} пар` : ' · нет пар');
  btnPrev.disabled = activeIndex === 0;
  btnNext.disabled = activeIndex === 6;
  btnPrev.classList.toggle('opacity-40', activeIndex === 0);
  btnNext.classList.toggle('opacity-40', activeIndex === 6);
}

function posFor(i) {
  const delta = i - activeIndex;
  if (delta >= -2 && delta <= 2) return String(delta);
  return 'hidden';
}

/** Only update data-pos — CSS transition handles the rest */
function updateAlbumPositions() {
  const pages = scheduleContainer.querySelectorAll('.album-page');
  pages.forEach(el => {
    const i = +el.dataset.index;
    el.dataset.pos = posFor(i);
  });
  updateDayHeader();
}

function buildAlbumDOM() {
  if (!weekData) return;
  dayNav.classList.remove('hidden');
  dayNav.classList.add('flex');

  const pages = weekData.days.map((day, i) => {
    const todayBadge = day.isToday
      ? '<span class="ml-1 text-[10px] font-medium px-1.5 py-0.5 rounded-full bg-white/20">сегодня</span>'
      : '';
    return `
      <div class="album-page" data-index="${i}" data-pos="${posFor(i)}">
        <div class="h-full bg-white dark:bg-slate-900 rounded-2xl border border-slate-200 dark:border-slate-800
                    shadow-lg dark:shadow-black/40 overflow-hidden flex flex-col">
          <div class="px-4 py-3 border-b border-slate-100 dark:border-slate-800 flex items-baseline justify-between
                      ${day.isToday ? 'bg-hub-600 text-white' : 'bg-slate-50 dark:bg-slate-800/80'}">
            <span class="font-semibold text-sm">${day.weekday}${todayBadge}</span>
            <span class="text-xs opacity-80">${day.dayNum} ${day.month}</span>
          </div>
          <div class="p-3 flex-1 overflow-y-auto max-h-[55vh]">${renderDayBody(day)}</div>
        </div>
      </div>`;
  }).join('');

  scheduleContainer.innerHTML = `
    <div class="album relative" style="height: min(560px, 70vh);">
      <div class="album-track relative w-full h-full">${pages}</div>
    </div>`;

  scheduleContainer.querySelectorAll('.album-page').forEach(el => {
    el.addEventListener('click', () => {
      const i = +el.dataset.index;
      if (i !== activeIndex) {
        activeIndex = i;
        updateAlbumPositions();
      }
    });
  });
  albumBuilt = true;
  updateDayHeader();
}

function setActiveDay(delta) {
  if (!weekData) return;
  const next = activeIndex + delta;
  if (next < 0 || next > 6) return;
  activeIndex = next;
  if (albumBuilt) updateAlbumPositions();
  else buildAlbumDOM();
}

btnPrev.addEventListener('click', () => setActiveDay(-1));
btnNext.addEventListener('click', () => setActiveDay(1));

let touchX = null;
scheduleContainer.addEventListener('touchstart', e => { touchX = e.changedTouches[0].screenX; }, { passive: true });
scheduleContainer.addEventListener('touchend', e => {
  if (touchX == null) return;
  const dx = e.changedTouches[0].screenX - touchX;
  touchX = null;
  if (Math.abs(dx) < 40) return;
  setActiveDay(dx < 0 ? 1 : -1);
}, { passive: true });

document.addEventListener('keydown', e => {
  if (panelSchedule.classList.contains('hidden')) return;
  if (!weekPicker.classList.contains('hidden')) return;
  if (e.key === 'ArrowLeft') setActiveDay(-1);
  if (e.key === 'ArrowRight') setActiveDay(1);
});

// Week picker
function openWeekPicker() {
  if (!weekMonday) return;
  const opts = buildWeekOptions();
  weekList.innerHTML = opts.map(o => {
    const active = o.monIso === weekMonday;
    return `
      <button type="button" data-week="${o.monIso}"
              class="w-full text-left px-3 py-2.5 rounded-xl text-sm transition
                     ${active ? 'bg-hub-600 text-white shadow-sm' : 'hover:bg-slate-100 dark:hover:bg-slate-800 text-slate-700 dark:text-slate-200'}">
        <span class="font-medium block">${o.label}</span>
        <span class="text-xs ${active ? 'text-white/80' : 'text-slate-400'}">${o.range}</span>
      </button>`;
  }).join('');
  weekPicker.classList.remove('hidden');
  weekList.querySelectorAll('[data-week]').forEach(btn => {
    btn.addEventListener('click', () => {
      const mon = btn.dataset.week;
      weekPicker.classList.add('hidden');
      if (mon !== weekMonday) {
        weekMonday = mon;
        loadSchedule(true, mon);
      }
    });
  });
}
btnWeekPick.addEventListener('click', e => {
  e.stopPropagation();
  if (weekPicker.classList.contains('hidden')) openWeekPicker();
  else weekPicker.classList.add('hidden');
});
document.addEventListener('click', e => {
  if (!weekPicker.classList.contains('hidden') &&
      !weekPicker.contains(e.target) &&
      e.target !== btnWeekPick && !btnWeekPick.contains(e.target)) {
    weekPicker.classList.add('hidden');
  }
});

function buildWeekFromResponse(data, mondayIso) {
  const days = weekDatesFromMonday(mondayIso || data.from_date);
  const map = {};
  (data.days || []).forEach(d => {
    map[d.date] = (d.lessons || []).slice().sort((a, b) =>
      (a.begin_time || '').localeCompare(b.begin_time || '')
    );
  });
  days.forEach(d => { d.lessons = map[d.iso] || []; });
  return {
    group: data.group, from: data.from_date, to: data.to_date,
    from_cache: data.from_cache, lesson_count: data.lesson_count, days,
  };
}

function pickInitialIndex(days) {
  const todayIdx = days.findIndex(d => d.isToday);
  if (todayIdx >= 0) return todayIdx;
  const withLessons = days.findIndex(d => d.lessons.length);
  return withLessons >= 0 ? withLessons : 0;
}

function renderSchedule(data) {
  weekData = buildWeekFromResponse(data, weekMonday);
  activeIndex = pickInitialIndex(weekData.days);
  albumBuilt = false;
  const lc = weekData.lesson_count ?? weekData.days.reduce((s, d) => s + d.lessons.length, 0);
  scheduleMeta.textContent =
    `Группа ${weekData.group} · ${weekData.from} — ${weekData.to}` +
    (weekData.from_cache ? ' · кэш' : ' · live') + ` · пар: ${lc}`;
  buildAlbumDOM();
}

async function loadSchedule(forceRefresh = true, mondayIso = null) {
  const group = groupInput.value.trim();
  if (!group) {
    scheduleContainer.innerHTML = '<p class="text-slate-400 text-center py-16 text-sm">Введите номер группы</p>';
    dayNav.classList.add('hidden'); dayNav.classList.remove('flex');
    return;
  }
  localStorage.setItem(LS_GROUP, group);
  weekMonday = mondayIso || weekMonday || toISO(mondayOf(new Date()));
  const from = weekMonday;
  const to = toISO(addDays(new Date(weekMonday + 'T12:00:00'), 6));

  scheduleContainer.innerHTML = `
    <div class="flex justify-center py-20">
      <div class="w-8 h-8 border-2 border-hub-400 border-t-transparent rounded-full animate-spin"></div>
    </div>`;
  albumBuilt = false;

  try {
    let url = `${API}/api/schedule?group=${encodeURIComponent(group)}&from=${from}&to=${to}`;
    if (forceRefresh) url += '&refresh=1';
    const res = await fetch(url);
    if (!res.ok) throw new Error(await res.text());
    renderSchedule(await res.json());
  } catch (e) {
    scheduleContainer.innerHTML = `<p class="text-red-500 text-center py-12 text-sm">Ошибка: ${escapeHtml(e.message)}</p>`;
  }
}

btnLoad.addEventListener('click', () => {
  weekMonday = toISO(mondayOf(new Date()));
  loadSchedule(true);
});
groupInput.addEventListener('keydown', e => {
  if (e.key === 'Enter') {
    weekMonday = toISO(mondayOf(new Date()));
    loadSchedule(true);
  }
});
if (savedGroup) {
  weekMonday = toISO(mondayOf(new Date()));
  loadSchedule(true);
}

// Refresh past-lesson graying every minute
setInterval(() => {
  if (weekData && albumBuilt) {
    // rebuild only active day content is heavy; full rebuild positions preserved
    const idx = activeIndex;
    buildAlbumDOM();
    activeIndex = idx;
    updateAlbumPositions();
  }
}, 60_000);

// ===================== BARS =====================
const loginForm = document.getElementById('login-form');
const loginError = document.getElementById('login-error');
const authBox = document.getElementById('auth-box');
const gradesBox = document.getElementById('grades-box');
const gradesContainer = document.getElementById('grades-container');
const btnLogout = document.getElementById('btn-logout');

function getToken() { return localStorage.getItem(LS_TOKEN); }
function showGradesUI(on) {
  authBox.classList.toggle('hidden', on);
  gradesBox.classList.toggle('hidden', !on);
}

async function loadGrades() {
  if (!getToken()) { showGradesUI(false); return; }
  showGradesUI(true);
  gradesContainer.innerHTML = '<p class="text-slate-400 text-center py-8 text-sm">Загрузка…</p>';
  try {
    const res = await fetch(`${API}/api/bars/grades`, {
      headers: { Authorization: `Bearer ${getToken()}` }
    });
    if (res.status === 401) { localStorage.removeItem(LS_TOKEN); showGradesUI(false); return; }
    if (!res.ok) throw new Error(await res.text());
    const data = await res.json();
    if (!data.subjects?.length) {
      gradesContainer.innerHTML = '<p class="text-slate-400 text-center py-8">Нет данных</p>';
      return;
    }
    gradesContainer.innerHTML = `
      <div class="grid gap-3 sm:grid-cols-2 fade-in">
        ${data.subjects.map(s => {
          const pct = s.max_score > 0 ? Math.min(100, Math.round(100 * s.score / s.max_score)) : 0;
          return `
            <div class="bg-white dark:bg-slate-900 rounded-2xl border border-slate-200 dark:border-slate-800 p-4 hover:shadow-md transition">
              <div class="flex justify-between gap-2 mb-2">
                <h3 class="font-semibold text-sm leading-snug">${escapeHtml(s.name)}</h3>
                <span class="text-sm font-bold text-hub-700 dark:text-hub-400 tabular-nums whitespace-nowrap">${s.score}/${s.max_score}</span>
              </div>
              <div class="h-2 bg-slate-100 dark:bg-slate-800 rounded-full overflow-hidden mb-2">
                <div class="h-full bg-hub-500 rounded-full transition-all duration-500" style="width:${pct}%"></div>
              </div>
              ${s.grade ? `<span class="text-xs text-slate-500">Оценка: ${escapeHtml(s.grade)}</span>` : ''}
            </div>`;
        }).join('')}
      </div>`;
  } catch (e) {
    gradesContainer.innerHTML = `<p class="text-red-500 text-center py-8">${escapeHtml(e.message)}</p>`;
  }
}

loginForm.addEventListener('submit', async e => {
  e.preventDefault();
  loginError.classList.add('hidden');
  try {
    const res = await fetch(`${API}/api/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        username: document.getElementById('login-user').value,
        password: document.getElementById('login-pass').value
      })
    });
    if (!res.ok) throw new Error((await res.json()).error || 'Ошибка входа');
    localStorage.setItem(LS_TOKEN, (await res.json()).token);
    showGradesUI(true);
    loadGrades();
  } catch (err) {
    loginError.textContent = err.message;
    loginError.classList.remove('hidden');
  }
});
btnLogout.addEventListener('click', () => {
  localStorage.removeItem(LS_TOKEN);
  showGradesUI(false);
});
if (getToken()) loadGrades();
