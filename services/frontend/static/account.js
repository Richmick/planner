(() => {
	const form = document.querySelector(".auth-form");
	const registering = form.id === "register-form";
	const alternate = document.getElementById(registering ? "login-link" : "reg-link");
	const feedback = document.getElementById("error-place");
	const submit = form.querySelector(".auth-submit");
	const toggle = form.querySelector(".auth-toggle");
	const password = form.elements.password;
	const confirmation = form.elements["password-confirm"];
	const submitLabel = submit.textContent;

	const messages = {
		FAILED: "Неверный логин или пароль.",
		NICKNAME_BUSY: "Этот логин уже занят.",
		EMAIL_BUSY: "Эта почта уже используется.",
		PW_UNSUPPORTED: "Введите другой пароль.",
		PW_TOO_SHORT: "Пароль слишком короткий.",
		DB_ERROR: "Сервис временно недоступен. Попробуйте позже.",
		INTERNAL_ERROR: "Сервис временно недоступен. Попробуйте позже.",
		MISSING_OBLIGATORY: registering ? "Заполните логин, почту и пароль." : "Введите логин и пароль."
	};

	function getBackUrl() {
		const candidate = new URLSearchParams(location.search).get("backurl");
		if (!candidate) return "/";
		try {
			const url = new URL(candidate, location.origin);
			if (url.origin === location.origin && !url.pathname.startsWith("//")) {
				return url.pathname + url.search + url.hash;
			}
		} catch {
			return "/";
		}
		return "/";
	}

	const backUrl = getBackUrl();
	document.getElementById("backurl-link").href = backUrl;
	if (backUrl !== "/") {
		const alternateUrl = new URL(alternate.href);
		alternateUrl.searchParams.set("backurl", backUrl);
		alternate.href = alternateUrl;
	}

	function showFeedback(message, success = false) {
		feedback.textContent = message;
		feedback.hidden = !message;
		feedback.classList.toggle("is-success", success);
	}

	toggle.addEventListener("click", () => {
		const visible = password.type === "password";
		password.type = visible ? "text" : "password";
		if (confirmation) confirmation.type = password.type;
		toggle.setAttribute("aria-pressed", String(visible));
		toggle.textContent = visible ? "Скрыть" : "Показать";
	});

	async function postAccount(body) {
		const options = {
			method: "POST",
			headers: { "Content-Type": "application/json" },
			credentials: "same-origin",
			signal: AbortSignal.timeout(10_000),
			body
		};
		const response = await fetch(form.action, options);
		if (response.status === 419) return fetch(form.action, options);
		return response;
	}

	async function responseMessage(response) {
		if (response.status === 419) return "Сессия истекла. Обновите страницу и повторите попытку.";
		if (response.status === 421 || response.status >= 500) return "Сервис временно недоступен. Попробуйте позже.";
		const payload = await response.json().catch(() => null);
		return messages[payload?.error?.code] || (registering
			? "Не удалось создать аккаунт. Проверьте введённые данные."
			: "Не удалось войти. Проверьте логин и пароль.");
	}

	form.addEventListener("submit", async (event) => {
		event.preventDefault();
		if (submit.disabled) return;
		if (confirmation && confirmation.value !== password.value) {
			showFeedback("Пароли не совпадают.");
			confirmation.focus();
			return;
		}

		showFeedback("");
		submit.disabled = true;
		submit.textContent = registering ? "Создаём аккаунт…" : "Входим…";
		let redirecting = false;
		try {
			const body = JSON.stringify(Object.fromEntries(new FormData(form)));
			const response = await postAccount(body);
			if (!response.ok) {
				showFeedback(await responseMessage(response));
				return;
			}
			if (registering) {
				showFeedback("Аккаунт создан. Открываем страницу входа…", true);
				redirecting = true;
				setTimeout(() => location.replace(alternate.href), 1400);
				return;
			}
			const account = await response.json().catch(() => null);
			if (!account?.id) {
				showFeedback("Не удалось подтвердить вход. Попробуйте ещё раз.");
				return;
			}
			localStorage.setItem("user-id", account.id);
			location.replace(backUrl);
			redirecting = true;
		} catch (error) {
			showFeedback(error.name === "TimeoutError" || error.name === "AbortError"
				? "Сервер не ответил вовремя. Попробуйте ещё раз."
				: "Не удалось связаться с сервером. Проверьте подключение.");
		} finally {
			if (!redirecting) {
				submit.disabled = false;
				submit.textContent = submitLabel;
			}
		}
	});
})();
