(function () {
  "use strict";
  const tags = {
    "04A10110": ["Abertura da noite", "Luzes da cidade", "Último refrão"],
    "04A10220": ["Um lado do disco"]
  };
  const labels = { off: "Desligado", idle: "Aguardando tag", playing: "Tocando", ended: "Encerrado", fault: "Falha" };
  const initial = () => ({ status: "off", uid: "", selected: "", index: -1, volume: 10, fault: "", sd: true, reader: true, note: "" });
  function act(state, action, value) {
    const next = { ...state, note: "" };
    if (action === "off") return { ...initial(), sd: state.sd, reader: state.reader };
    if (action === "sd" || action === "reader") {
      next[action] = value;
      if (!value && state.status !== "off" && action === "sd") {
        Object.assign(next, { status: "fault", index: -1, fault: "Cartão microSD indisponível" });
      }
      return next;
    }
    if (action === "start") {
      const fault = !state.reader ? "RC522 sem comunicação" : !state.sd ? "Cartão microSD indisponível" : "";
      return { ...initial(), sd: state.sd, reader: state.reader, status: fault ? "fault" : "idle", fault };
    }
    if (state.status === "off" || state.status === "fault") return next;
    if (action === "scan" && state.reader) {
      next.uid = value;
      if (!Object.hasOwn(tags, value)) next.note = "Tag desconhecida. Reprodução preservada.";
      else if (value !== state.selected) Object.assign(next, { selected: value, index: 0, status: "playing" });
    }
    if (action === "volume" && Number.isInteger(value) && value >= 0 && value <= 20) next.volume = value;
    if (state.status === "playing") {
      if ((action === "next" || action === "finish") && state.index + 1 < tags[state.selected].length) next.index++;
      else if (action === "finish") Object.assign(next, { status: "ended", index: -1 });
    }
    if (action === "replay" && state.status === "ended") Object.assign(next, { status: "playing", index: 0 });
    return next;
  }
  const playing = [["start"], ["scan", "04A10110"]];
  const cases = [
    ["01", "Energizar a bancada", [["start"]], "Aguardando tag"],
    ["02", "Aproximar a tag do álbum", playing, "Tocando · Abertura da noite"],
    ["03", "Avançar pelo painel", [...playing, ["next"]], "Tocando · Luzes da cidade"],
    ["04", "Receber fim de faixa pela UART", [...playing, ["finish"], ["finish"]], "Tocando · Último refrão"],
    ["05", "Encerrar a playlist", [...playing, ["finish"], ["finish"], ["finish"]], "Encerrado"],
    ["06", "Repetir a faixa avulsa", [["start"], ["scan", "04A10220"], ["finish"], ["replay"]], "Tocando · Um lado do disco"],
    ["07", "Alterar o volume para 15", [...playing, ["volume", 15]], "Tocando · Abertura da noite · Volume 15"],
    ["08", "Ler tag desconhecida durante a faixa", [...playing, ["scan", "DEADBEEF"]], "Tocando · Abertura da noite · Tag desconhecida. Reprodução preservada."],
    ["09", "Retirar microSD durante a reprodução", [...playing, ["sd", false]], "Falha · Cartão microSD indisponível"],
    ["10", "Inicializar com o RC522 desconectado", [["reader", false], ["start"]], "Falha · RC522 sem comunicação"],
    ["11", "Reinstalar microSD e reiniciar", [...playing, ["sd", false], ["sd", true], ["start"]], "Aguardando tag"],
    ["12", "Desligar durante a reprodução", [...playing, ["off"]], "Desligado"],
    ["13", "Reler a mesma tag após avançar", [...playing, ["next"], ["scan", "04A10110"]], "Tocando · Luzes da cidade"],
    ["14", "Inicializar sem microSD", [["sd", false], ["start"]], "Falha · Cartão microSD indisponível"],
    ["15", "Trocar para a tag avulsa durante o álbum", [...playing, ["scan", "04A10220"]], "Tocando · Um lado do disco"]
  ];
  function describe(s) {
    return [labels[s.status], s.fault, tags[s.selected]?.[s.index], s.volume !== 10 ? "Volume " + s.volume : "", s.note].filter(Boolean).join(" · ");
  }
  function run() {
    return cases.map(([id, test, steps, expected]) => {
      const actual = describe(steps.reduce((s, [action, value]) => act(s, action, value), initial()));
      return { id, test, expected, actual, pass: actual === expected };
    });
  }
  if (typeof module === "object") module.exports = { run };
  if (typeof document === "undefined") return;
  const root = document.querySelector("#operacao");
  const results = document.querySelector("#resultados");
  function showResults() {
    const rows = run();
    results.replaceChildren(...rows.map(row => {
      const tr = document.createElement("tr");
      for (const value of [row.id, row.test, row.expected, row.actual, row.pass ? "Passou" : "Falhou"]) {
        const td = document.createElement("td");
        td.textContent = value;
        tr.append(td);
      }
      return tr;
    }));
    document.querySelector("#placar").textContent = rows.filter(row => row.pass).length + "/" + rows.length + " casos passaram";
  }
  showResults();
  document.querySelector("#retestar").addEventListener("click", showResults);
  let state = initial();
  function render() {
    root.querySelector("output").textContent = describe(state);
    root.querySelector("#volume-modelo").value = state.volume;
    root.querySelectorAll("[data-action]").forEach(button => {
      const action = button.dataset.action;
      if (action === "next") button.disabled = state.status !== "playing" || state.index + 1 >= tags[state.selected].length;
      if (action === "finish") button.disabled = state.status !== "playing";
      if (action === "replay") button.disabled = state.status !== "ended";
      if (action === "scan" || action === "volume") button.disabled = !["idle", "playing", "ended"].includes(state.status);
    });
  }
  root.addEventListener("click", event => {
    const button = event.target.closest("button[data-action]");
    if (!button) return;
    state = act(state, button.dataset.action, button.dataset.value);
    render();
  });
  root.addEventListener("input", event => {
    const { action } = event.target.dataset;
    if (!action) return;
    state = act(state, action, action === "volume" ? Number(event.target.value) : event.target.checked);
    render();
  });
  render();
})();
