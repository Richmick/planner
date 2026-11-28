function setError(msg) {
	let err = document.getElementById("error-place");
	if (!err) {
		alert(msg);
		return;
	}
	err.textContent = msg;
	err.classList.add("feedback");
	err.classList.add("error");
}
function showError(e, code) {
	if (typeof (e) == "string") setError(e);
	else if (e.msg) setError(e.msg);
	else if (e.error) {
		if (typeof (e.error) == "string") setError(e.error);
		else if (e.error.code && dict && dict[e.error.code]) {
			setError(dict[e.error.code]);
		} else if (e.error.msg) setError(e.error.msg);
		else setError("Нераспознанная ошибка (Код " + code + "): " + e.error);
	} else setError("Нераспознанный ответ от сервера (Код " + code + "): " + e);
}
(() => {
	const login = document.getElementById("login-link");
	const logout = document.getElementById("logout-form");
	if (!login || !logout) return;

	for (const link of document.querySelectorAll(".site-links a")) {
		if (link.pathname === location.pathname) link.setAttribute("aria-current", "page");
	}
	if (location.pathname !== "/") {
		const returnTo = location.pathname + location.search;
		login.href = "/login?" + new URLSearchParams({ backurl: returnTo });
	}

	const signedIn = Boolean(localStorage.getItem("user-id"));
	login.hidden = signedIn;
	logout.hidden = !signedIn;
	logout.addEventListener("submit", async (event) => {
		event.preventDefault();
		const button = logout.querySelector("button");
		const feedback = document.getElementById("site-feedback");
		button.disabled = true;
		feedback.hidden = true;
		try {
			const response = await fetch("/api/logout", { method: "POST", credentials: "same-origin" });
			if (!response.ok && response.status !== 401 && response.status !== 419) {
				throw new Error("Не удалось выйти из аккаунта. Попробуйте ещё раз.");
			}
			localStorage.removeItem("user-id");
			window.location.replace("/login");
		} catch {
			feedback.textContent = "Не удалось выйти из аккаунта. Попробуйте ещё раз.";
			feedback.hidden = false;
			button.disabled = false;
		}
	});
})();
