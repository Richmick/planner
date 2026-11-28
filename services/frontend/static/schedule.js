(() => {
	const minutesPerDay = 24 * 60;
	const minute = 60_000;
	const weekday = new Intl.DateTimeFormat("ru-RU", { weekday: "short" });
	const dayLabel = new Intl.DateTimeFormat("ru-RU", { weekday: "long", day: "numeric", month: "long" });
	const periodLabel = new Intl.DateTimeFormat("ru-RU", { day: "numeric", month: "long", year: "numeric" });

	const dateKey = (date) => {
		const month = String(date.getMonth() + 1).padStart(2, "0");
		const day = String(date.getDate()).padStart(2, "0");
		return `${date.getFullYear()}-${month}-${day}`;
	};

	const midnight = (date) => {
		const start = new Date(date);
		start.setHours(0, 0, 0, 0);
		return start;
	};

	const addDays = (date, days) => {
		const result = midnight(date);
		result.setDate(result.getDate() + days);
		return result;
	};

	const minutesFromMidnight = (date) => date.getHours() * 60 + date.getMinutes();
	const timeLabel = (minutes) => {
		const hours = String(Math.floor(minutes / 60) % 24).padStart(2, "0");
		return `${hours}:${String(minutes % 60).padStart(2, "0")}`;
	};

	const parseTime = (value) => {
		if (typeof value !== "string") return null;
		const date = new Date(value);
		return Number.isNaN(date.getTime()) ? null : date;
	};

	const localTime = (day, time) => {
		const date = parseTime(`${day}T${time}`);
		return date && dateKey(date) === day && timeLabel(minutesFromMidnight(date)) === time ? date : null;
	};

	const element = (tag, className, text) => {
		const node = document.createElement(tag);
		if (className) node.className = className;
		if (text !== undefined) node.textContent = text;
		return node;
	};

	function visibleDays(anchor, weekly) {
		if (!weekly) return [anchor];
		const monday = addDays(anchor, -((anchor.getDay() + 6) % 7));
		return Array.from({ length: 7 }, (_, index) => addDays(monday, index));
	}

	function readState(key) {
		try {
			const data = JSON.parse(localStorage.getItem(key));
			return {
				tasks: (data?.tasks ?? []).filter((task) => typeof task?.id === "string" && typeof task.name === "string"),
				placements: new Map(Object.entries(data?.placements ?? {}))
			};
		} catch {
			return { tasks: [], placements: new Map() };
		}
	}

	function eventTasks(event) {
		if (event?.event_id == null) return [];
		const prefix = `event:${event.event_id}`;
		const name = event.name || "Задача";
		const tasks = [];
		const slots = Array.isArray(event.scheduled) ? event.scheduled : [];
		for (const [index, slot] of slots.entries()) {
			const start = parseTime(slot?.start);
			const end = parseTime(slot?.end);
			if (start && end && end > start) {
				tasks.push({ key: `${prefix}:${index}`, name, interval: { start, end }, finished: event.finished });
			}
		}
		if (!tasks.length) tasks.push({ key: `${prefix}:0`, name, interval: null, finished: event.finished });
		return tasks;
	}

	function placeInLanes(blocks) {
		blocks.sort((left, right) => left.from - right.from || right.to - left.to);
		let group = [];
		let laneEnds = [];
		let groupEnd = 0;
		const finishGroup = () => {
			for (const block of group) block.lanes = laneEnds.length;
			group = [];
			laneEnds = [];
		};
		for (const block of blocks) {
			if (block.from >= groupEnd) finishGroup();
			const free = laneEnds.findIndex((end) => end <= block.from);
			block.lane = free === -1 ? laneEnds.length : free;
			laneEnds[block.lane] = block.to;
			groupEnd = Math.max(groupEnd, block.to);
			group.push(block);
		}
		finishGroup();
	}

	function makeDialog() {
		const dialog = element("dialog", "schedule-dialog");
		dialog.setAttribute("aria-labelledby", "schedule-move-title");
		dialog.innerHTML = `
			<form>
				<h2 id="schedule-move-title"></h2>
				<div class="schedule-dialog-fields">
					<label>Дата <input id="schedule-move-date" type="date" required></label>
					<label>Время <input id="schedule-move-time" type="time" required></label>
				</div>
				<p id="schedule-move-error" class="calendar-feedback error" role="alert" hidden></p>
				<div class="schedule-dialog-actions">
					<button id="schedule-move-save" class="button-primary" type="submit">Запланировать</button>
					<button id="schedule-move-pool" type="button">В пул задач</button>
					<button id="schedule-remove-task" class="button-danger" type="button">Удалить задачу</button>
					<button id="schedule-move-cancel" type="button">Отмена</button>
				</div>
			</form>`;
		document.body.append(dialog);
		return dialog;
	}

	window.initSchedule = ({ getJson, describeError }) => {
		const grid = document.getElementById("schedule-grid");
		const scroll = document.getElementById("schedule-scroll");
		const pool = document.getElementById("schedule-pool-list");
		const poolCount = document.getElementById("schedule-pool-count");
		const status = document.getElementById("schedule-status");
		const period = document.getElementById("schedule-period");
		const weekButton = document.getElementById("schedule-week");
		const dayButton = document.getElementById("schedule-day");
		const taskForm = document.getElementById("schedule-task-form");
		const taskInput = document.getElementById("schedule-task-name");
		const dialog = makeDialog();
		const moveDate = dialog.querySelector("#schedule-move-date");
		const moveTime = dialog.querySelector("#schedule-move-time");
		const moveError = dialog.querySelector("#schedule-move-error");
		const poolButton = dialog.querySelector("#schedule-move-pool");
		const removeButton = dialog.querySelector("#schedule-remove-task");
		const storageKey = "planner.schedule." + (localStorage.getItem("user-id") || "guest");
		const { tasks: localTasks, placements } = readState(storageKey);
		let weekly = !matchMedia("(max-width: 700px)").matches;
		let anchor = midnight(new Date());
		let datedEvents = [];
		let activeEvents = [];
		let taskIndex = new Map();
		let selected = null;
		let dragged = null;
		let dropTarget = null;
		let periodRequest = 0;
		let activeRequest = 0;
		let firstRender = true;
		let renderPending = false;

		function setStatus(message, error = false) {
			status.textContent = message;
			status.hidden = !message;
			status.classList.toggle("error", error);
		}

		function persist() {
			try {
				localStorage.setItem(storageKey, JSON.stringify({ tasks: localTasks, placements: Object.fromEntries(placements) }));
			} catch {
				setStatus("Расстановка не сохранилась. Браузер не разрешил запись.", true);
			}
		}

		function effectiveInterval(task) {
			if (task.finished) return task.interval;
			const placement = placements.get(task.key);
			if (placement === null) return null;
			const start = parseTime(placement);
			if (!start) return task.interval;
			const duration = task.interval ? task.interval.end - task.interval.start : 30 * minute;
			return { start, end: new Date(start.getTime() + duration) };
		}

		function collectTasks() {
			const events = new Map();
			for (const event of [...activeEvents, ...datedEvents]) events.set(event?.event_id, event);
			const tasks = Array.from(events.values()).flatMap(eventTasks);
			for (const { id, name } of localTasks) tasks.push({ key: `local:${id}`, name, local: true, interval: null });
			return new Map(tasks.map((task) => [task.key, task]));
		}

		function makeCard(task, className, time) {
			const card = element("button", className);
			card.type = "button";
			card.draggable = !task.finished;
			card.disabled = Boolean(task.finished);
			card.dataset.taskKey = task.key;
			card.classList.toggle("is-local", Boolean(task.local));
			card.classList.toggle("is-finished", Boolean(task.finished));
			card.title = time ? `${time} · ${task.name}` : task.name;
			if (time) card.append(element("span", "schedule-event-time", time));
			card.append(element("span", "schedule-event-name", task.name));
			return card;
		}

		function dayParts(tasks, day) {
			const endOfDay = addDays(day, 1);
			const blocks = [];
			for (const { task, interval } of tasks) {
				if (!interval || interval.start >= endOfDay || interval.end <= day) continue;
				const from = interval.start <= day ? 0 : minutesFromMidnight(interval.start);
				const to = interval.end >= endOfDay ? minutesPerDay : minutesFromMidnight(interval.end);
				// A short interval can occupy the same wall-clock minute or span a clock adjustment.
				const drawnEnd = Math.min(minutesPerDay, Math.max(from + 20, to));
				blocks.push({ task, from, to: drawnEnd, time: `${timeLabel(from)}–${timeLabel(to)}` });
			}
			placeInLanes(blocks);
			return blocks;
		}

		function makeTrack(day, tasks, isToday) {
			const track = element("div", "schedule-day-track");
			track.dataset.date = dateKey(day);
			track.classList.toggle("is-today", isToday);
			for (const block of dayParts(tasks, day)) {
				const card = makeCard(block.task, "schedule-event", block.time);
				card.classList.toggle("is-compact", block.to - block.from < 45);
				const width = 100 / block.lanes;
				card.style.top = `calc(var(--hour-size) * ${block.from / 60})`;
				card.style.height = `calc(var(--hour-size) * ${(block.to - block.from) / 60})`;
				card.style.left = `calc(${width * block.lane}% + 3px)`;
				card.style.width = `calc(${width}% - 6px)`;
				track.append(card);
			}
			return track;
		}

		function render() {
			if (dragged) {
				renderPending = true;
				return;
			}
			renderPending = false;
			taskIndex = collectTasks();
			const tasks = Array.from(taskIndex.values(), (task) => ({ task, interval: effectiveInterval(task) }));
			const days = visibleDays(anchor, weekly);
			const today = dateKey(new Date());
			weekButton.setAttribute("aria-pressed", String(weekly));
			dayButton.setAttribute("aria-pressed", String(!weekly));
			period.textContent = weekly ? periodLabel.formatRange(days[0], days.at(-1)) : periodLabel.format(anchor);
			grid.style.setProperty("--days", days.length);
			grid.dataset.view = weekly ? "week" : "day";

			const heading = element("div", "schedule-heading");
			heading.append(element("div", "schedule-corner", "Время"));
			const body = element("div", "schedule-body");
			const hours = element("div", "schedule-hours");
			for (let hour = 0; hour < 24; ++hour) {
				hours.append(element("div", "schedule-hour", timeLabel(hour * 60)));
			}
			body.append(hours);

			for (const day of days) {
				const key = dateKey(day);
				const isToday = key === today;
				const label = element("button", "schedule-day-label");
				label.type = "button";
				label.dataset.date = key;
				label.setAttribute("aria-label", dayLabel.format(day));
				label.classList.toggle("is-today", isToday);
				label.append(element("span", "schedule-weekday", weekday.format(day)));
				label.append(element("span", "schedule-date", day.getDate()));
				heading.append(label);
				body.append(makeTrack(day, tasks, isToday));
			}
			grid.replaceChildren(heading, body);
			if (firstRender) {
				const hourSize = hours.firstElementChild.getBoundingClientRect().height;
				scroll.scrollTop = 8 * hourSize;
				firstRender = false;
			}

			const freeTasks = tasks.filter(({ interval }) => !interval);
			pool.replaceChildren();
			if (poolCount) poolCount.textContent = freeTasks.length;
			if (!freeTasks.length) pool.append(element("p", "schedule-pool-empty", "Перетащите сюда задачу, чтобы освободить время."));
			for (const { task } of freeTasks) pool.append(makeCard(task, "schedule-pool-task"));
		}

		async function loadPeriod() {
			const request = ++periodRequest;
			datedEvents = [];
			render();
			setStatus("Загрузка расписания…");
			const days = visibleDays(anchor, weekly);
			// The API interprets date filters in Moscow. Include adjacent dates for local-time display.
			const query = new URLSearchParams({
				from: dateKey(addDays(days[0], -1)),
				to: dateKey(addDays(days.at(-1), 1))
			});
			try {
				const events = await getJson(`/api/calendars/events?${query}`);
				if (request !== periodRequest) return;
				if (!Array.isArray(events)) throw new SyntaxError("Expected an event array");
				datedEvents = events;
				render();
				setStatus("");
			} catch (error) {
				if (request !== periodRequest) return;
				if ([401, 403, 419].includes(error.status)) {
					++activeRequest;
					activeEvents = [];
					render();
				}
				setStatus(describeError(error), true);
			}
		}

		async function refreshActive() {
			const request = ++activeRequest;
			try {
				const events = await getJson("/api/calendars/events/active");
				if (request === activeRequest && Array.isArray(events)) {
					activeEvents = events;
					render();
				}
			} catch {
				// Dated events still work if the optional active-events request fails.
			}
		}

		function refresh() {
			return Promise.all([loadPeriod(), refreshActive()]);
		}

		function setView(nextDate, nextWeekly = weekly) {
			anchor = midnight(nextDate);
			weekly = nextWeekly;
			scroll.scrollLeft = 0;
			return loadPeriod();
		}

		function taskFrom(target) {
			const card = target.closest("[data-task-key]");
			return card ? taskIndex.get(card.dataset.taskKey) : null;
		}

		function openTask(task) {
			selected = task;
			const interval = effectiveInterval(task);
			dialog.querySelector("#schedule-move-title").textContent = task.name;
			moveDate.value = dateKey(interval?.start ?? anchor);
			moveTime.value = interval ? timeLabel(minutesFromMidnight(interval.start)) : "09:00";
			moveError.hidden = true;
			poolButton.hidden = !interval;
			removeButton.hidden = !task.local;
			dialog.showModal();
		}

		function moveTask(task, start) {
			placements.set(task.key, start?.toISOString() ?? null);
			persist();
			render();
		}

		function setDropTarget(target) {
			if (target === dropTarget) return;
			dropTarget?.classList.remove("is-drop-target");
			dropTarget = target;
			dropTarget?.classList.add("is-drop-target");
		}

		function dropTask(event, target) {
			if (!dragged) return;
			event.preventDefault();
			const task = dragged;
			dragged = null;
			setDropTarget(null);
			if (target === pool) {
				moveTask(task, null);
				return;
			}
			const rect = target.getBoundingClientRect();
			const fraction = (event.clientY - rect.top) / rect.height;
			const startMinute = Math.min(1425, Math.max(0, Math.round(fraction * 96) * 15));
			const start = localTime(target.dataset.date, timeLabel(startMinute));
			if (start) moveTask(task, start);
			else setStatus("В этот день выбранное время недоступно.", true);
		}

		function enableDrop(node, findTarget) {
			node.addEventListener("dragstart", (event) => {
				dragged = taskFrom(event.target);
				if (!dragged) return;
				event.dataTransfer.effectAllowed = "move";
				event.dataTransfer.setData("text/plain", dragged.key);
			});
			node.addEventListener("dragover", (event) => {
				if (!dragged) return;
				const target = findTarget(event.target);
				setDropTarget(target);
				if (!target) return;
				event.preventDefault();
				event.dataTransfer.dropEffect = "move";
			});
			node.addEventListener("dragleave", (event) => {
				const related = event.relatedTarget;
				if (dropTarget && (!(related instanceof Node) || !dropTarget.contains(related))) setDropTarget(null);
			});
			node.addEventListener("drop", (event) => {
				const target = findTarget(event.target);
				if (target) dropTask(event, target);
			});
		}

		grid.addEventListener("click", (event) => {
			const task = taskFrom(event.target);
			if (task) {
				openTask(task);
				return;
			}
			const label = event.target.closest(".schedule-day-label");
			if (label && weekly) setView(parseTime(label.dataset.date + "T00:00"), false);
		});
		pool.addEventListener("click", (event) => {
			const task = taskFrom(event.target);
			if (task) openTask(task);
		});
		enableDrop(grid, (target) => target.closest(".schedule-day-track"));
		enableDrop(pool, () => pool);
		document.addEventListener("dragend", () => {
			dragged = null;
			setDropTarget(null);
			if (renderPending) render();
		});

		dialog.querySelector("form").addEventListener("submit", (event) => {
			event.preventDefault();
			if (!selected) return;
			const start = localTime(moveDate.value, moveTime.value);
			if (!start) {
				moveError.textContent = "Выберите время, которое существует в этот день.";
				moveError.hidden = false;
				return;
			}
			const days = visibleDays(anchor, weekly);
			const task = selected;
			dialog.close();
			moveTask(task, start);
			if (start < days[0] || start >= addDays(days.at(-1), 1)) setView(start);
		});

		poolButton.addEventListener("click", () => {
			const task = selected;
			dialog.close();
			if (task) moveTask(task, null);
		});
		removeButton.addEventListener("click", () => {
			if (!selected?.local) return;
			const index = localTasks.findIndex(({ id }) => `local:${id}` === selected.key);
			if (index === -1) return;
			localTasks.splice(index, 1);
			placements.delete(selected.key);
			dialog.close();
			persist();
			render();
		});
		dialog.querySelector("#schedule-move-cancel").addEventListener("click", () => dialog.close());
		dialog.addEventListener("close", () => { selected = null; });

		taskForm.addEventListener("submit", (event) => {
			event.preventDefault();
			const name = taskInput.value.trim();
			if (!name) return;
			const id = crypto.randomUUID?.() ?? Date.now().toString(36) + Math.random().toString(36).slice(2);
			localTasks.push({ id, name });
			taskForm.reset();
			persist();
			render();
		});

		weekButton.addEventListener("click", () => { if (!weekly) setView(anchor, true); });
		dayButton.addEventListener("click", () => { if (weekly) setView(anchor, false); });
		document.getElementById("schedule-prev").addEventListener("click", () => setView(addDays(anchor, weekly ? -7 : -1)));
		document.getElementById("schedule-next").addEventListener("click", () => setView(addDays(anchor, weekly ? 7 : 1)));
		document.getElementById("schedule-today").addEventListener("click", () => setView(new Date()));

		refresh();
		return { refresh };
	};
})();
