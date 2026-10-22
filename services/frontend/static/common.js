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