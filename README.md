# МЭИ Student App

Веб-приложение для студентов НИУ «МЭИ»: расписание занятий (RUZ) + успеваемость из БАРС.

## Стек

| Слой | Технология |
|------|------------|
| Backend | C++17, **Crow** (HTTP), libcurl, nlohmann/json |
| DB | SQLite (кэш расписания + сессии) |
| Frontend | HTML5 + Tailwind CSS (CDN) + vanilla JS |
| Сборка | CMake ≥ 3.16 |

## Структура

```
.
├── CMakeLists.txt
├── README.md
├── include/                  # общие заголовки
│   ├── models.hpp
│   ├── ruz_client.hpp
│   ├── bars_client.hpp
│   ├── schedule_cache.hpp
│   └── database.hpp
├── backend/
│   ├── main.cpp
│   ├── controllers/          # REST-роуты
│   ├── services/             # RUZ / BARS клиенты, кэш
│   └── db/
└── frontend/
    ├── index.html
    └── js/app.js
```

## API

| Метод | Путь | Описание |
|-------|------|----------|
| `GET` | `/api/schedule?group=ГРУППА` | Расписание на текущую неделю (опционально `&from=&to=`) |
| `POST` | `/api/auth/login` | `{ "login": "...", "password": "..." }` → токен |
| `GET` | `/api/bars/grades` | Оценки (заголовок `Authorization: Bearer <token>`) |
| `POST` | `/api/auth/logout` | Инвалидация токена |

## Сборка

### Зависимости (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev libsqlite3-dev
```

Crow и nlohmann/json подтягиваются через `FetchContent` при первой конфигурации.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./mpei_app 8080
```

Откройте http://localhost:8080

## Важные замечания по интеграции

### RUZ (ts.mpei.ru)

Официальный публичный REST-контракт МЭИ не опубликован. Клиент пробует типичные для российских вузов эндпоинты:

- `/api/search/groups?term=...`
- `/api/schedule/group/{oid}?start=YYYY.MM.DD&finish=YYYY.MM.DD`
- `/ruzservice.svc/personLessons?...`

Если live-запрос не удаётся, возвращается **mock-занятие**, чтобы UI можно было разрабатывать офлайн.

Для продакшена откройте DevTools на https://ts.mpei.ru/ruz/main, найдите реальные XHR/JSON-запросы и поправьте URL/поля в `backend/services/ruz_client.cpp`.

### БАРС (bars.mpei.ru)

Авторизация — корпоративная учётная запись ОСЭП (форма + cookies).  
Клиент реализует proxy-паттерн: логин → cookie → внутренний Bearer-токен → SQLite-сессия.

Поля формы и точные URL кабинета оценок нужно уточнить по Network-вкладке браузера и обновить:

- `BarsClient::login`
- `BarsClient::fetch_grades`

При неуспешном live-логине (нет cookies) используется mock-сессия, чтобы фронтенд работал.

### Безопасность

- Пароли **не** сохраняются — только cookie БАРС (на 12 ч) и наш токен.
- В продакшене поставьте TLS (nginx/caddy) и не храните cookie в plaintext без шифрования.
- 2FA (Telegram/VK) в skeleton не реализована — при её включении потребуется доп. шаг.

## Дальнейшее развитие

1. Разобрать реальные эндпоинты RUZ/БАРС и заменить mock.
2. Добавить поиск групп / преподавателей / аудиторий.
3. Кэш с фоновым обновлением (APScheduler-подобный таймер).
4. Docker-образ + docker-compose.
5. JWT вместо random-токена, refresh-токены.
6. PWA / мобильная обёртка.
