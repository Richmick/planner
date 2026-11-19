(() => {
    const calendarsNode = document.getElementById("calendar-list");
    const eventsNode = document.getElementById("event-list");
    const errorNode = document.getElementById("calendar-error");
    const groupForm = document.getElementById("calendar-form");
    const dateForm = document.getElementById("date-form");
    const activeButton = document.getElementById("active-events");

    const localDate = (date) => {
        const year = date.getFullYear();
        const month = String(date.getMonth() + 1).padStart(2, "0");
        const day = String(date.getDate()).padStart(2, "0");
        return `${year}-${month}-${day}`;
    };

    const showError = (message) => {
        errorNode.textContent = message;
        errorNode.hidden = !message;
    };

    const request = async (url, options = {}) => {
        const response = await fetch(url, {
            credentials: "same-origin",
            ...options,
            headers: {
                "Content-Type": "application/json",
                ...(options.headers || {})
            }
        });
        const payload = response.status === 204 ? null : await response.json();
        if (!response.ok) {
            const error = payload?.error?.msg || payload?.error || `Ошибка запроса: ${response.status}`;
            throw new Error(error);
        }
        return payload;
    };

    const createText = (tag, value, className) => {
        const node = document.createElement(tag);
        node.textContent = value;
        if (className) node.className = className;
        return node;
    };

    const renderCalendars = (calendars) => {
        calendarsNode.replaceChildren();
        if (!calendars.length) {
            calendarsNode.append(createText("p", "Календари ещё не добавлены."));
            return;
        }
        for (const calendar of calendars) {
            const name = calendar.metadata?.name || calendar.external_id;
            const item = createText("div", `${name} · ${calendar.source_type}`, "calendar-item");
            calendarsNode.append(item);
        }
    };

    const renderEvents = (events) => {
        eventsNode.replaceChildren();
        if (!events.length) {
            eventsNode.append(createText("p", "На выбранный период задач нет."));
            return;
        }
        for (const event of events) {
            const item = document.createElement("article");
            item.className = "event-item";
            item.append(createText("h3", event.name));
            const intervals = Array.isArray(event.scheduled) ? event.scheduled : [];
            for (const interval of intervals) {
                item.append(createText("p", `${interval.start || ""} — ${interval.end || ""}`));
            }
            const status = event.finished ? "Завершено" : "Активно";
            item.append(createText("span", status, "event-status"));
            eventsNode.append(item);
        }
    };

    const loadCalendars = async () => {
        const calendars = await request("/api/calendars/calendars");
        renderCalendars(calendars);
    };

    const loadEvents = async () => {
        const from = document.getElementById("date-from").value;
        const to = document.getElementById("date-to").value;
        const query = new URLSearchParams({ from, to });
        const events = await request(`/api/calendars/events?${query}`);
        renderEvents(events);
    };

    groupForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        showError("");
        const group = document.getElementById("group-name").value.trim();
        try {
            await request("/api/calendars/calendars", {
                method: "POST",
                body: JSON.stringify({ source_type: "ruz", name: group })
            });
            document.getElementById("group-name").value = "";
            await loadCalendars();
            await loadEvents();
        } catch (error) {
            showError(error.message);
        }
    });

    dateForm.addEventListener("submit", async (event) => {
        event.preventDefault();
        showError("");
        try {
            await loadEvents();
        } catch (error) {
            showError(error.message);
        }
    });

    activeButton.addEventListener("click", async () => {
        showError("");
        try {
            const events = await request("/api/calendars/events/active");
            renderEvents(events);
        } catch (error) {
            showError(error.message);
        }
    });

    const today = new Date();
    document.getElementById("date-from").value = localDate(today);
    document.getElementById("date-to").value = localDate(today);
    loadCalendars().catch((error) => showError(error.message));
    loadEvents().catch((error) => showError(error.message));
})();
