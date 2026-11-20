(() => {
	const calendarsUrl = "/api/calendars/calendars";
	const calendarsNode = document.getElementById("calendar-list");
	const eventsNode = document.getElementById("event-list");
	const errorNode = document.getElementById("calendar-error");
	const groupForm = document.getElementById("calendar-form");
	const dateForm = document.getElementById("date-form");
	const activeButton = document.getElementById("active-events");
	const openForm = document.getElementById("open-calendar-form");
	const calendarIdInput = document.getElementById("calendar-id");
	const openButton = openForm.querySelector("button");
	const detailNode = document.getElementById("calendar-details");
	const detailError = document.getElementById("calendar-detail-error");
	const detailStatus = document.getElementById("calendar-detail-status");
	const accessForm = document.getElementById("calendar-access-request");
	const accessButton = accessForm.querySelector("button");
	const accessStatus = document.getElementById("access-request-status");
	const grantForm = document.getElementById("notification-grant");
	const grantStatus = document.getElementById("grant-status");
	let protectedCalendar = null;

	class HttpError extends Error {
		constructor(status, message) {
			super(message || `Ошибка запроса (${status}).`);
			this.status = status;
		}
	}

	const describeError = (error) => {
		const status = error.status;
		if (error.name === "TimeoutError" || error.name === "AbortError") {
			return "Сервер не ответил вовремя. Попробуйте ещё раз.";
		}
		if (status === 408 || status === 504) {
			return `Сервер не ответил вовремя (${status}). Попробуйте ещё раз.`;
		}
		if (status === 401 || status === 419) return `Войдите в аккаунт и повторите запрос (${status}).`;
		if (status === 404) return "Календарь не найден (404).";
		if (status === 403) return "У вас нет доступа к этому календарю (403).";
		if (status === 421) return "Сервис временно недоступен (421).";
		if (status >= 500) return `Ошибка сервера (${status}). Попробуйте ещё раз.`;
		if (error instanceof SyntaxError) return "Сервер вернул ответ в неожиданном формате.";
		if (error instanceof TypeError) return "Не удалось связаться с сервером. Проверьте подключение.";
		return error.message;
	};

	const setFeedback = (node, message, isError = false) => {
		node.textContent = message;
		node.hidden = !message;
		node.classList.toggle("error", isError);
	};

	const request = async (url, { method = "GET", body } = {}) => {
		const headers = { Accept: "application/json" };
		if (body !== undefined) headers["Content-Type"] = "application/json";
		const response = await fetch(url, {
			method,
			headers,
			body: body === undefined ? undefined : JSON.stringify(body),
			credentials: "same-origin",
			cache: "no-store",
			signal: AbortSignal.timeout(10_000)
		});
		if (response.ok) return response;
		let message;
		if ([400, 409, 422].includes(response.status)) {
			const payload = await response.json().catch(() => null);
			message = typeof payload?.error === "string" ? payload.error : payload?.error?.msg;
		}
		throw new HttpError(response.status, message);
	};

	const getJson = async (url) => (await request(url)).json();

	const positiveId = (value) => {
		const id = Number(value);
		return /^[1-9]\d*$/.test(String(value)) && Number.isSafeInteger(id) ? id : null;
	};

	const localDate = (date) => {
		const year = date.getFullYear();
		const month = String(date.getMonth() + 1).padStart(2, "0");
		const day = String(date.getDate()).padStart(2, "0");
		return `${year}-${month}-${day}`;
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
			const item = createText("div", "", "calendar-item");
			const id = positiveId(calendar.calendar_id);
			if (id) {
				const link = createText("a", name);
				link.href = `/calendar?calendar_id=${id}`;
				item.append(link, ` · ${calendar.source_type}`);
			} else {
				item.textContent = `${name} · ${calendar.source_type}`;
			}
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

	const openCalendar = async (value) => {
		if (openButton.disabled) return;
		protectedCalendar = null;
		accessForm.hidden = true;
		detailNode.hidden = true;
		setFeedback(accessStatus, "");
		setFeedback(detailError, "");
		const id = positiveId(value);
		if (!id) {
			setFeedback(detailError, "Введите корректный ID календаря.", true);
			return;
		}

		const pageUrl = new URL(location.href);
		pageUrl.searchParams.set("calendar_id", id);
		history.replaceState(null, "", pageUrl);
		openButton.disabled = true;
		setFeedback(detailStatus, "Загрузка календаря…");
		try {
			const calendar = await getJson(`${calendarsUrl}/${id}`);
			if (!calendar || typeof calendar !== "object" || Array.isArray(calendar)) {
				throw new SyntaxError("Expected a calendar object");
			}
			const name = calendar.metadata?.name || calendar.name || calendar.external_id || `Календарь №${id}`;
			detailNode.replaceChildren(createText("h3", name));
			const description = calendar.metadata?.description || calendar.description;
			if (description) detailNode.append(createText("p", description));
			detailNode.hidden = false;
		} catch (error) {
			if (error.status === 403) {
				protectedCalendar = { id };
				accessForm.reset();
				accessButton.disabled = false;
				accessForm.hidden = false;
			}
			setFeedback(detailError, describeError(error), true);
		} finally {
			openButton.disabled = false;
			setFeedback(detailStatus, "");
		}
	};

	openForm.addEventListener("submit", (event) => {
		event.preventDefault();
		openCalendar(calendarIdInput.value);
	});

	accessForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		const target = protectedCalendar;
		if (!target || accessButton.disabled) return;
		const userId = positiveId(localStorage.getItem("user-id"));
		if (!userId) {
			setFeedback(accessStatus, "Войдите в аккаунт, чтобы запросить доступ.", true);
			return;
		}

		accessButton.disabled = true;
		setFeedback(accessStatus, "Отправка запроса…");
		try {
			await request("/api/notifier/events", {
				method: "POST",
				body: {
					calendar_id: target.id,
					user_id: userId,
					event: "access-request",
					access: document.getElementById("requested-access").value
				}
			});
			if (target === protectedCalendar) setFeedback(accessStatus, "Запрос на доступ отправлен.");
		} catch (error) {
			if (target !== protectedCalendar) return;
			const message = error.status === 404 ? "Адрес сервиса уведомлений не найден (404)." : describeError(error);
			setFeedback(accessStatus, `Не удалось отправить запрос. ${message}`, true);
			accessButton.disabled = false;
		}
	});

	grantForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		const button = grantForm.querySelector("button");
		if (button.disabled) return;
		const { calendarId, userId, access } = grantForm.dataset;
		button.disabled = true;
		setFeedback(grantStatus, "Предоставление доступа…");
		try {
			await request(`${calendarsUrl}/${calendarId}/access`, {
				method: "POST",
				body: { user_id: Number(userId), access }
			});
			setFeedback(grantStatus, "Доступ предоставлен.");
		} catch (error) {
			const message = error.status === 403 ? "У вас нет права выдавать доступ (403)." : describeError(error);
			setFeedback(grantStatus, `Не удалось предоставить доступ. ${message}`, true);
			button.disabled = false;
		}
	});

	const loadCalendars = async () => renderCalendars(await getJson(calendarsUrl));

	const loadEvents = async () => {
		const from = document.getElementById("date-from").value;
		const to = document.getElementById("date-to").value;
		const query = new URLSearchParams({ from, to });
		renderEvents(await getJson(`/api/calendars/events?${query}`));
	};

	const showError = (error) => setFeedback(errorNode, describeError(error), true);

	groupForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		setFeedback(errorNode, "");
		const groupName = document.getElementById("group-name");
		try {
			await request(calendarsUrl, {
				method: "POST",
				body: { source_type: "ruz", name: groupName.value.trim() }
			});
			groupName.value = "";
			await loadCalendars();
			await loadEvents();
		} catch (error) {
			showError(error);
		}
	});

	dateForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		setFeedback(errorNode, "");
		try {
			await loadEvents();
		} catch (error) {
			showError(error);
		}
	});

	activeButton.addEventListener("click", async () => {
		setFeedback(errorNode, "");
		try {
			renderEvents(await getJson("/api/calendars/events/active"));
		} catch (error) {
			showError(error);
		}
	});

	const today = new Date();
	document.getElementById("date-from").value = localDate(today);
	document.getElementById("date-to").value = localDate(today);
	loadCalendars().catch(showError);
	loadEvents().catch(showError);
	const initialId = new URLSearchParams(location.search).get("calendar_id");
	if (initialId !== null) {
		calendarIdInput.value = initialId;
		openCalendar(initialId);
	}
})();
