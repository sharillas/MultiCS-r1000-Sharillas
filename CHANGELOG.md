## v1.70.19 (2026-11-24) — `DCW AGREE` deixa de bloquear com `DCW SWAP` / `DCW CAK7`
- **Correção**: com `DCW SWAP: YES` ou `DCW CAK7: YES` num perfil com `DCW AGREE: 2` (ou mais), a box ficava sem nenhuma CW (`decode failed to CCcam client`, `agree: so 0/2`). O consenso comparava a CW **já transformada** com as CWs **cruas** guardadas de cada reader, por isso nunca havia acordo e o ECM ficava retido até ao timeout.
- A transformação de saída (CAK7 / CAK7 INVERSE / SWAP) passou para uma única função, aplicada tanto à CW recebida como às cópias dos readers antes de o `agree` as comparar. Sem SWAP/CAK7 o comportamento é igual ao anterior.
- Dado de campo que motivou o teste do SWAP no NOS (1802): nas CWs entregues, o ECM de tabela `0x80` alterava a **segunda** metade e o de tabela `0x81` a **primeira**, ao contrário do esperado (`0x80` = metade par, bytes 0-7); a box recebia a CW nova no slot de paridade trocado (oscam com `found`, ecrã preto).
- Testado no sandbox (gcc, gnu90): compila sem erros e regressão func_test 110/110.

## v1.70.18 (2026-11-24) — reader com `nochecksum=yes`
- **Nova opção por reader `nochecksum=yes`** (checkbox **"No CW checksum"** no popup Edit dos Servers): as CWs que este reader devolve deixam de ser rejeitadas por checksum inválido. Aplica-se às respostas de ECM de todos os tipos de reader (newcamd, cccam, camd35, cs378x, radegast, ccam3) e, nos perfis com NAGRA protection, também à rejeição `CHK` / "CWLR: CW lixo (checksum)" quando a CW vem desse reader. **Continuam a ser rejeitadas** as CWs nulas, "bad" e fake (último byte XOR 0xF0). Não altera o CacheEX nem os outros readers, e não mexe no `dcw.c`. Desmarcar apaga a opção do ficheiro (não fica comentada). É independente do `nocheck` (esse só desliga a proteção anti-loop de IP). Por omissão fica desligada.
- Para experimentar num reader que entrega CWs com checksum fora do padrão: marcar "No CW checksum" no Edit do reader (ou escrever `nochecksum=yes;` no bloco `{ }` da linha). O reload automático aplica sem restart.
- Testado no sandbox (gcc, gnu90, gdb): 12/12 no popup (checkbox, gravar, reload, desmarcar apaga, edição manual lida, `nocheck` independente), função de aceitação com CW boa / checksum mau / nula / fake, e regressões 48/48, 21/21, 27/27, 110/110, 30/30, 9/9, 12/12 e 25/25. O teste ponta-a-ponta com um reader real que entregue CW com checksum mau fica para a produção.

## v1.70.17 (2026-11-24) — Monitoring: Reader, Hop, gráfico 24h e servers removidos
- **Coluna Reader (Anti-Share Protection, Readers e Active bad-CW cache)**: passa a mostrar o `name` do server se estiver configurado, senão só o host — sem o "-" e sem o "(host:porta)" que aparecia na tabela de bad-CW. Na Anti-Share a coluna chama-se agora "Reader" (IP e porta continuam nas suas colunas).
- **Servers que já não estão na config deixavam de aparecer só ao reiniciar**: quando uma linha de server passava a comentada à mão no ficheiro (`#N:`, `#C:`, `#CS378X SERVER:`, `#CAMD35 SERVER:`), o reload automático mantinha o server antigo ativo em memória (e a aparecer no Monitoring) até ao restart. Agora o reload segue o ficheiro: comentar desativa (e desliga) o server, descomentar volta a ativá-lo. O Monitoring também deixa de listar servers removidos ou desativados.
- **Coluna Hop mais clara**: o Hop é a opção `hop=N` da linha do server (1 = fonte direta, preferida pelo load balancer; 2+ = circuito, i.e. as cartas chegam pela cadeia desse server). O "?" era só "opção não definida" — agora diz `not set`, mostra `direct` ou `circuit (N)`, com um tooltip a explicar.
- **Last 24 h com outro desenho**: sai o gráfico SVG; entra a percentagem de ECMs OK das últimas 24 h + uma tira de 96 janelas de 15 min (mais recente à direita) colorida pelo design da GUI (verde ≥90 %, amarelo ≥60 %, vermelho abaixo, vazio = sem pedidos; marca vermelha no topo = houve cwbad). Cada janela tem tooltip com OK/total.
- Testado no sandbox (gcc, gnu90, gdb): 12/12 novos (labels, Hop, tira de 96 células, servers comentados/removidos desaparecem do Monitoring, 0 erros JS) e regressões 27, 21, 48, 110, 30, 9 e 25 verdes.

## v1.70.16 (2026-11-24) — Autorefresh estilo OSCam (polling em todas as páginas)
- **Novo mecanismo de polling, igual ao do OSCam WebIf**: em vez de um temporizador fixo que substituía todo o conteúdo, cada página pede o seu `action=div` ao servidor e **atualiza o ecrã no sítio** (as linhas/células existentes são alteradas, não recriadas). Resultado: sem piscar, a ordenação escolhida na tabela mantém-se, as linhas de debug abertas e o texto que estás a escrever não se perdem, e passar o rato por cima da tabela já não bloqueia a atualização. O pedido seguinte só é agendado depois da resposta chegar (nunca há pedidos sobrepostos).
- **Controlos na barra (como no OSCam)**: `−` / valor / `+` para o intervalo (1–99 s), botão **Pause polling / Resume polling** (mostra `--` em pausa), e o valor pisca a cada resposta recebida (heartbeat). O intervalo e a pausa ficam guardados no browser e **mantêm-se ao mudar de página**. O intervalo inicial continua a vir de `HTTP AUTOREFRESH` do cfg.
- **Aviso POLLERR**: se o servidor não responder (ou a sessão expirar) aparece um aviso vermelho e o polling tenta de novo ao fim de 15 s; desaparece sozinho quando volta a responder. Sessão expirada redireciona para o login.
- **Todas as páginas passam a atualizar**: Monitoring (`/watchdog`) e a página de Cache Peer não tinham autorefresh efetivo; agora têm. A pausa também aplica-se ao texto do Debug Log (continua congelado enquanto estiver em Stop live log).
- Testado no sandbox (gcc, gnu90, gdb): 30/30 na auditoria de todas as páginas do menu (polling ativo, −/+, pausa/retoma, persistência entre páginas, POLLERR e recuperação, rato por cima não bloqueia, mesmos elementos DOM, ordenação mantida, 0 erros JS), 9/9 nos casos de dados (Monitoring muda sozinho, disable/enable refletido sem recarregar, linha nova/removida, input com foco intacto) e regressões 27/27 (debug), 21/21, 48/48, 25/25 e 110/110 da GUI.

## v1.70.15 (2026-11-23) — Monitoring: parar o debug em tempo real
- **Botão "Stop live log" no Debug Log do Monitoring (e na página Debug)**: o log atualiza-se a cada 2 s e é substituído pela atualização da página, o que tornava impossível ler com calma. O botão congela o log no ecrã ("Resume live log" retoma). O estado fica visível ao lado ("Live" / "Paused"). Com o log parado, as restantes secções da página continuam a atualizar-se (só o bloco do log é preservado); mudar o filtro (ALL/ERROR/...) retoma o modo live. É só da interface — não altera o nível de debug do servidor.
- **Fix — a página Monitoring fazia crash do processo quando não havia nenhum Cache Server configurado** (`cache_peers()` desreferenciava NULL na secção Cache Stats). Em produção havia cache peers, por isso passava despercebido; agora devolve 0/0.
- Testado no sandbox (gcc, gnu90, build -g com gdb): 27/27 testes do botão em /watchdog e /debug (label, aria-pressed, estado, log congelado inclusive depois de updateDiv, retoma, mudança de filtro, 0 erros JS) e confirmação de que o log muda com a pausa desligada e fica congelado com ela ligada enquanto o servidor continua a gerar linhas.

## v1.70.14 (2026-11-23) — validação do #56 em produção
- **Fix — clientes CacheEX (CS378X/CAMD35) davam "Client not found" no popup Edit, e o OFF/ON/DEL não faziam nada**: os clientes com `cacheex_mode>0` ficam numa lista própria (`cacheexclient`) e `getcs378xclientbyid`/`getcamd35clientbyid` e o handler dos botões da lista só percorriam a lista normal. Corrigido nos três sítios.
- **Popups de cliente com checkboxes e opções reais**: `DCW Check` como checkbox, `CacheEX mode` como seleção (Off / 1 / 2 / 3, nos tipos F:/CS378X/CAMD35) e "Outras opções" **pré-preenchido** com o que a linha já tem (`profiles`, `name`, `host`, `expire`, `shares`…) — antes aparecia vazio e gravar apagava tudo. O campo posicional do `F:` (reshare) é preservado e cada tipo mantém a sua keyword (`F:`/`MG:`/`CS378X USER:`/`CAMD35 USER:`/`USER:`; antes um cliente MG/CS378X/CAMD35 gravado pelo popup virava `USER:`). `dcwcheck` só é escrito quando difere do default do protocolo (default ligado em F:/MG:/USER:, desligado em CS378X/CAMD35), por isso desmarcar num protocolo com default ligado grava `dcwcheck=no`.
- **Popup de Server também pré-preenche as opções reais**: antes só reconstruía hop/budget/profiles a partir da memória, e `name="…"`, `sids`, `priority`, `shares`, `type`, `cacheex_maxhop/forward` desapareciam ao gravar sem alterações. A whitelist do campo livre passou a incluir todas as chaves que o parser aceita, e é consciente de aspas (`name="A;B"`).
- **Fix grave (existia desde a v1.70.13) — save do popup truncado em silêncio**: o parser HTTP limita o URL a ~511 caracteres; acima disso os últimos parâmetros eram cortados e a resposta continuava "OK" — opções longas ficavam gravadas incompletas e as checkboxes/selects (no fim do URL) perdiam-se. O save passou a ir por **POST** (corpo sem esses limites), com marcador final obrigatório `end=1` (pedido sem ele é recusado como incompleto) e campos longos demais recusados em vez de truncados. Também corrigido um overflow de stack potencial no decode do campo "Outras opções" (buffer de 640 sem limite). Linhas com opções demasiado longas para o popup (> ~1900 chars) recusam abrir e remetem para o Editor Configs, sem alterar nada.
- **Fix — o popup mostrava o estado antigo das opções de Server depois de gravar**: a recarga da config (`update_servers`) não transportava `nocheck`/`providrewrite`/`stealth`/`arbiter`/`hop`/`budget_*`/`name` para o server já existente, por isso desmarcar uma checkbox só tinha efeito no ficheiro e a checkbox continuava marcada até ao restart. Agora são transportados.
- **Erros do save visíveis**: o popup fechava sempre que o servidor respondia 200, escondendo as mensagens de rejeição (chave desconhecida, etc.). Agora só fecha em sucesso e mostra o erro dentro do popup.
- **Visual**: checkboxes alinhadas com o texto nos popups (`label.le-cb`).
- **Edição de linhas mais segura**: o match de `USER:`/`MG:`/`F:`/`CS378X USER:`/`CAMD35 USER:` passou a ser ancorado ao início da linha (antes `strstr`: um comentário de exemplo ou um user com o mesmo nome noutro tipo podia ser editado/apagado em vez da linha real).
- Testado no sandbox (gcc, gnu90) com fixtures realistas: 25/25 (checkboxes/whitelist), 48/48 (clientes: CacheEX, round-trip sem perda F:/MG:/USER:/CS378X/CAMD35, dcwcheck, select CacheEX, ON/OFF/DEL, match ancorado), 21/21 (servidores: round-trip, edição parcial, `;` entre aspas, sids de ~1400 chars, recusa de pedidos enormes) e 110/110 da regressão completa da GUI; 0 erros JS.

## v1.70.13 (2026-11-23) — #56
- **Editor popup `/editline` — checkboxes reais em vez de texto livre**: as opções booleanas de cada tipo de linha passam a ter checkboxes dedicadas: Server (`nocheck`/`providrewrite`/`stealth`/`arbiter`), clientes newcamd/cccam/mgcamd/CS378X/CAMD35 (`dcwcheck`), Cache Peer (`csp`/`autoadd`/`sendreq`/`sendrep`/`fwd` + o campo posicional `fblock0onid`). Antes era preciso escrever `key=value;` à mão em "Outras opções" — um erro de digitação partia o parse do resto da linha (ou de todo o ficheiro, nos tipos sem fallback seguro). Agora: uma opção desmarcada é **sempre removida por completo** do texto gravado (nunca comentada, nunca `key=no`); uma opção só é escrita se estiver marcada. O campo "Outras opções" passa a validar por whitelist nos tipos sem fallback seguro no parser (Servers, clientes CS378X/CAMD35, Cache Peer): qualquer chave desconhecida **rejeita a gravação inteira** (nada é escrito) com aviso ao utilizador, em vez de arriscar corromper o ficheiro.
- **Fix — DesKey aparecia em todos os Clientes**: o campo DesKey (14 pares) só faz sentido em Servers newcamd; deixa de aparecer (incorretamente) em qualquer tipo de Cliente no popup de edição.
- **Cache Peer — popup de Edit novo**: o Cache Peer nunca tinha popup de edição próprio. Agora tem (Host/Porta, checkboxes de opções, "Outras opções" com whitelist).
- **Fix — Cache Peer ON/OFF e DELETE sem efeito persistente**: o botão de desativar/ativar só alterava o estado em memória, nunca comentava/descomentava a linha `CACHE PEER:` no `Cache.cfg` (revertia no reload seguinte); o botão DEL nem era reconhecido (caía sempre na página normal). Corrigidos ambos. Além disso, `CACHE PEER:` passou a ser reconhecida como linha "desativável" pelo parser (tal como `N:`/`C:`/`CS378X SERVER:`/`CAMD35 SERVER:` desde a v1.54) — sem isto, um peer desativado pela GUI desaparecia por completo da memória no reload automático seguinte, em vez de ficar listado como inativo.
- **Fix — `dcwcheck` nunca era lido nas linhas `CS378X USER:`/`CAMD35 USER:`**: o campo já existia na struct e já era consumido em runtime (`srv-cs378x.c`/`srv-camd35.c`), mas nunca era parseado da linha de configuração — ficava sempre desligado e não configurável. Implementado o parse; corrigido também o merge do reload automático, que não transportava o valor lido para o cliente persistente (a gravação parecia funcionar mas revertia sozinha ~1-2s depois).
- **Fix — edição de clientes CCcam/mgcamd sem efeito quando a linha não está num ficheiro "...Clients.cfg"**: o editor de linha (save do popup) procurava sempre um ficheiro cujo *nome* contivesse "Clients.cfg" — em instalações que seguem a convenção de um ficheiro por protocolo (`clientes_cccam.cfg`, `clientes_mgcamd.cfg`, ver `configs_exemplos/`), a gravação não encontrava o ficheiro certo e ficava silenciosamente sem efeito (a GUI respondia OK na mesma). Corrigido: `config_line_edit`/`config_line_replace` procuram agora primeiro, em todos os ficheiros incluídos, qual **contém mesmo a linha** (por conteúdo) — só cai no nome do ficheiro como reserva quando nenhum tem a linha (necessário para quando se está mesmo a acrescentar uma linha nova).
- **Fix — tail de opções perdido ao gravar clientes mgcamd pelo popup**: a reconstrução da linha `MG:` esquecia sempre as opções gravadas, só escrevia user+pass.
- Testado no sandbox (gcc, gnu90): 25/25 testes dedicados (checkboxes por tipo, whitelist, dcwcheck round-trip em newcamd/cccam/mgcamd/CS378X/CAMD35, Cache Peer Edit/disable/enable/delete) + 111/111 do teste de regressão completo da GUI, 0 erros JS.

## v1.70.12 (2026-11-23)
- Fix (causa real do #53 "on/off e delete nao fazem nada" em cs378x/camd35): os botoes da
  pagina de lista real (/cs378x e /camd35) chamam http_send_c35_page() -- uma funcao
  partilhada entre os dois protocolos, DIFERENTE de http_send_cs378x_client/
  http_send_camd35_client (que a v1.70.10 tinha corrigido, mas que so' sao chamadas pelos
  endpoints /cs378xclient e /camd35client, nunca pelos botoes reais). http_send_c35_page()
  reconhecia "disable"/"enable" no parsing mas nao tinha NENHUM bloco a tratar essas accoes
  (so' existia handling para ACTION_ROW e ACTION_PAGE) -- o pedido AJAX devolvia sempre HTTP
  200 com a pagina/div re-renderizada, sem alterar nada, dando a impressao de sucesso. A
  accao "delete" nem sequer era reconhecida como string valida. Adicionado reconhecimento de
  "delete" e um bloco que processa ACTION_DISABLE/ACTION_ENABLE/ACTION_DELETE, procurando o
  cliente por id na lista de servidores ja' recebida pela funcao e persistindo a alteracao em
  Clients.cfg (tal como newcamd/cccam/mgcamd ja' faziam).
- Fix: botao Edit em clientes CS378X vistos na pagina de lista real mandava sempre
  type=camd35c (em vez de cs378xc) para o popup de edicao, porque camd35cells() detetava o
  protocolo comparando o "path" recebido com a string fixa "/cs378xclient" -- mas o path real
  usado pela lista e' "/cs378x" (c35_row_emit chama camd35cells com o path da pagina atual).
  O backend (/editline) acabava por procurar o cliente na lista de camd35 em vez de cs378x,
  dando quase sempre "Client not found" -- uma 2a causa, independente da do reload automatico
  (ja corrigida em v1.70.10), para o mesmo sintoma. Deteta agora cs378x por substring no path.
- Fix (causa raiz de ambos os bugs acima, mais profunda): cfg->camd35.clientid e
  cfg->cs378x.clientid nunca eram inicializados em init_config() (ficavam 0, via memset),
  ao contrario de newcamd/cccam/mgcamd (que comecam em 1). Em cfg_set_id_counters() -- que
  corre em muitos pedidos HTTP admin, nao so' no reload de config -- o teste usado para saber
  se um cliente ja tem id atribuido e' "if (!cli->id)", ou seja, trata id==0 como "ainda sem
  id". Mas e' exatamente o id que o PRIMEIRO cliente cs378x/camd35 de sempre recebe (o
  contador comeca em 0). Resultado: a cada chamada seguinte de cfg_set_id_counters(), esse
  1o cliente era silenciosamente re-numerado para um id novo (so' acontecia uma vez, porque o
  novo id ja nao e' zero) -- fazia o id capturado pelo browser no render anterior (usado pelos
  botoes e pelo popup Edit) deixar de corresponder ao cliente real, com o mesmo sintoma de
  "nao faz nada" / "Client not found". Contadores agora comecam em 1, como os outros
  protocolos, resolvendo a ambiguidade na origem.
- Adicionado: accao DELETE a http_send_cs378x_client/http_send_camd35_client (endpoints
  /cs378xclient e /camd35client) -- so' newcamd/cccam/mgcamd tinham; mantido por correcao
  mesmo nao sendo o caminho usado pelos botoes da lista principal.
- Verificado: suite de regressao completa 107/107, incluindo testes manuais end-to-end dos
  URLs reais usados pelos botoes (/cs378x?id=X&action=disable|enable|delete e equivalente
  para /camd35), confirmando persistencia em Clients.cfg e estabilidade do id do cliente
  entre pedidos.

## v1.70.11 (2026-11-23)
- Fix: pagina Servers, coluna "readers" -- quando o servidor nao tinha a opcao "name"
  configurada, mostrava sempre "-" em vez de cair para o host (ip/hostname usado na
  ligacao). Agora mostra o name se configurado, senao o host.
- Fix: pagina Monitoring, "Readers (reputation)" -- a coluna do reader mostrava sempre
  "- (host:port)" quando o servidor nao tinha "name" configurado (o "-" e os parenteses
  apareciam sempre, mesmo sem nome). Agora mostra o name se configurado, senao o host,
  sem o "-" nem os parenteses -- alinhado com a coluna "readers" da pagina Servers.

## v1.70.10 (2026-11-23)
- Fix: /newcamd e /cacheex perdiam o bloco "local services" (perfis Newcamd / servidores
  CacheEX) a cada refresh automatico ou logo apos clicar em qualquer botao on/off/delete.
  O bloco estava dentro do "if (get_action==ACTION_PAGE)", mas updateDiv()/autorefresh
  (action=div) substitui TODO o innerHTML de #mainDiv -- como esse pedido nunca incluia o
  bloco, ele "desaparecia" visualmente. Movido para fora do bloco ACTION_PAGE em ambas as
  paginas, para ser sempre incluido (http_send_newcamd / http_send_cacheex).
- Fix: cs378x/camd35 -- botoes on/off pareciam "nao fazer nada" e o popup de Edit dizia
  "Client not found" depois de desativar um cliente. Causa raiz: um cliente desativado
  (botao OFF) fica com a linha comentada no Clients.cfg; a thread de reload automatico
  (inotify, corre sempre que o ficheiro e gravado) ao reparsear nao via essa linha
  (comentada) e marcava o cliente, incorretamente, como removido (FLAG_DELETE) -- depois
  disso getcs378xclientbyid()/getcamd35clientbyid() deixavam de o encontrar. Corrigido em
  config.c: um cliente ja FLAG_DISABLE agora e' preservado no reload em vez de ser marcado
  para remocao por nao-match (mesmo padrao aplicado a update_cccam_clients,
  update_cccam_cacheexclients, update_mgcamd_clients, update_camd35_clients,
  update_camd35_cacheexclients e update_cardserver_newcamd_clients -- o bug era sistemico,
  presente em todos os protocolos, nao so' cs378x/camd35).
- Fix: getcs378xclientbyid() em srv-cs378x.c so' percorria o primeiro servidor de
  cfg.cs378x.server; com mais de um "CS378X PORT:" configurado, clientes dos servidores
  seguintes nunca eram encontrados por id. Alinhado com getcamd35clientbyid(), que ja
  percorria todos os servidores corretamente.
- Fix: http_send_cs378x_client/http_send_camd35_client faziam "return;" sem resposta HTTP
  quando o cliente nao era encontrado por id, deixando a ligacao AJAX pendurada (o browser
  nunca recebia status 200, nem nenhum outro). Agora redirecionam para /cs378x ou /camd35,
  alinhado com o padrao ja usado por newcamd/cccam/mgcamd.

## v1.70.9 (2026-11-23)
- Fix: same bug class as v1.70.8, found by auditing every protocol's GUI client handler in httpserver.c for
  the M:/MG: persistence pattern (http_send_cs378x_client at line ~1950 and http_send_camd35_client at line
  ~2329). Both ACTION_DISABLE/ACTION_ENABLE blocks only toggled cli->flags in memory and called
  cs378x_disconnect_cli()/camd35_disconnect_cli() -- neither ever called config_line_edit() to persist the
  change to Clients.cfg, unlike the already-correct newcamd ("USER: %s") and cccam ("F: %s") handlers.
  Effect: toggling Enable/Disable for a cs378x or camd35 (cacheex) client from the GUI worked only in memory
  -- after a restart or config-reread, a disabled client would come back enabled. Fixed by adding the same
  persistence block used for mgcamd in v1.70.8, with match strings "CS378X USER: %s" / "CAMD35 USER: %s",
  matching production Clients.cfg's real line format exactly (confirmed: /var/etc/Clients.cfg has
  "CS378X USER: vucex ..." and "CAMD35 USER: vucex35 ..." lines).
  Verified via a local sandbox build (gcc, same source): full GUI regression suite (func_test.js, all pages
  including cs378x/camd35) passed with no handler/JS regressions, and a live round-trip test -- added a
  throwaway test client for each protocol, called /cs378xclient?action=disable|enable and
  /camd35client?action=disable|enable via the HTTP API, and confirmed the matching line in Clients.cfg got
  commented/uncommented correctly each time (both reverted afterwards, no production/VPS config touched).
  No ACTION_DELETE exists for cs378x/camd35 (pre-existing GUI gap, out of scope for this fix -- the DEL
  button sends action=delete, which neither handler recognizes, so it is a no-op; kept consistent with the
  minimal, targeted scope of persisting only the existing Enable/Disable actions).

## v1.70.8 (2026-11-22)
- Fix: httpserver.c's mgcamd client Enable/Disable/Delete actions (3 sites in http_send_mgcamd_client,
  ACTION_ENABLE/ACTION_DISABLE/ACTION_DELETE) built the Clients.cfg match string as `sprintf(match, "M: %s",
  cli->user)`. But config.c's parser only ever writes/accepts mgcamd client lines as "MG:", "MGUSER:" or
  "MGCAMD USER:" -- never a bare "M:". Since config_line_edit() does a plain substring search, "M: <user>"
  never matched any real line in Clients.cfg (confirmed: none of the three valid line formats contain that
  substring). Effect: toggling Enable/Disable/Delete for an mgcamd client from the GUI worked only in memory
  (the live connection was dropped/flagged correctly) but was never persisted to Clients.cfg -- after a
  restart or config-reread, a disabled/deleted mgcamd client would come back enabled. This is the same
  pattern already used correctly elsewhere in the file (the inline edit-popup flow at line ~5458/5593 uses
  "MG: %s" and works). Not a security bug (admin-only GUI action), but a genuine functional one. Fixed by
  changing all 3 sites to "MG: %s", matching the parser and the other, already-correct code path.
- Investigated at the user's request: whether mgcamd (and the newcamd-family protocols generally) are still
  working correctly after freecccam/softcam were disabled, compared against OSCam's reference implementation.
  Reviewed srv-mgcamd.c's mg_connect_cli() (login handshake) and mg_cli_recvmsg() (EXT_GET_VERSION,
  MSG_CARD_DATA_REQ/EXT_ADD_CARD card advertisement, ECM request/response dispatch) end to end against OSCam's
  module-newcamd.c. Login-ACK magic bytes (sid=0x6E73, provider marker 0x14) and the EXT_GET_VERSION reply
  ("1.67") match OSCam's mgcamd-specific values exactly. One gap found: EXT_SID_LIST (0xD7), OSCam's optional
  per-SID deny-list hint, is defined as a constant but never sent by MultiCS -- not a functional break (the
  server still enforces SID-level access control at ECM-decode time via accept_sid()), just a missing
  client-side optimization hint. Verified live, not just by reading code: added a throwaway local test client
  (Mgcamd.cfg) and pointed Multics's own internal newcamd-client code at its own mgcamd server port via a
  loopback entry in Servers.cfg (both reverted afterwards, no production/VPS config touched). The loopback
  login, password verification, and card-discovery handshake succeeded end-to-end (5 CAID/provider cards
  correctly advertised via EXT_ADD_CARD), which is live confirmation -- not just static review -- that the
  core mgcamd protocol implementation works correctly.
- Verified: full GUI regression suite 109/109, clean compile. No live exploit or flood traffic was used for
  this investigation -- the mgcamd live check was a legitimate loopback login against the test build only.

## v1.70.7 (2026-11-22)
- Defensive fix: closed the 3 lower-priority items left from the earlier source review.
  1) add_host() in config.c did `strcpy(host->name, hostname)` into a fixed `char name[256]` with no length
     check; a long enough hostname value in Servers.cfg (or any future caller) would overflow it. Now uses
     strncpy bounded to sizeof(host->name)-1 plus an explicit NUL.
  2) 4 sites in httpserver.c built the "Servers.cfg"/"Clients.cfg" comment-toggle match string with
     `sprintf(match, "%s %d", host->name, port)` into a fixed `char match[160]`, with no bound against
     host->name's real (256-byte) capacity. Switched to snprintf(match, sizeof(match), ...).
  3) One XML-row endpoint in httpserver.c (the only one of 9 similar XML-cell endpoints in the file) still
     used a raw `sprintf(http_buf, ...)` into `char http_buf[4096]` built from 11 cell[2048] strings, instead
     of the snprintf the other 8 endpoints already use. Brought it in line with the rest of the file.
  All three require admin-level config/GUI access to reach (hostnames and usernames are admin-entered), not a
  remote/pre-auth attack surface like the earlier fixes in this series -- but each was a genuine missing bound,
  so closed for defense in depth. No behavior change for any realistically-sized value.
- Verified: full GUI regression suite 107/107, clean compile and clean run under ASan/UBSan.

## v1.70.6 (2026-11-22)
- Fix: telnet.c's 3 recv() sites (username, password, command loop) checked only `len<=0` before testing
  `buf[len-2]!='\r' || buf[len-1]!='\n'`. A client sending exactly 1 byte (len==1) made `len-2` negative,
  so `buf[len-2]` read one byte before the 4096-byte stack buffer -- an out-of-bounds read, reachable pre-auth
  (the very first recv, before any login) by simply opening a TCP connection to the telnet port and sending a
  single byte.
- Fixed by requiring `len>=2` at all 3 sites (a valid line must end in "\r\n", so anything shorter is already
  invalid input) instead of just `len>0`.
- Verified: full GUI regression suite 107/107, clean compile and clean start under ASan/UBSan. Telnet is not
  enabled in the test config, so this was verified by regression suite + build, not by a live telnet PoC; the
  fix itself is a straightforward bounds check (len>=2 before any buf[len-2] access).

## v1.70.5 (2026-11-22)
- Fix: config.c's line-parsing loop computed `pos = currentline + strlen(currentline) - 1` to trim trailing
  whitespace/CR/LF. When a config line becomes an empty string after comment-stripping (e.g. a line that is
  only a "# comment", or an already-blank line), strlen()==0 made this underflow (pointer arithmetic on
  `currentline - 1`), so `pos` pointed one byte before the line buffer -- an out-of-bounds read, and (if that
  adjacent byte happened to look like whitespace) a one-byte out-of-bounds write. Found incidentally by UBSan
  during normal config-reread while running the GUI regression suite; this path is only reachable by Multics's
  own config-file parser (not a remote attack surface), but is a genuine memory-safety bug.
- Fixed by skipping straight to the trim loop's safe "empty line" case (pos = currentline, pointing at the
  line's own NUL terminator) instead of computing strlen()-1 when the line is already empty.
- Verified: full GUI regression suite 107/107 (which exercises multiple config-reread paths), clean compile and
  clean run under ASan/UBSan -- the sanitizer report that used to fire during this suite no longer appears.

## v1.70.4 (2026-11-22)
- Critical security fix: the camd35/cs378x servers (UDP camd35_recvmsg, TCP cs378x_connect_cli/cs378x_msg_peek/
  cs378x_cli_recvmsg) compute an attacker-influenced message length from bytes that were just AES-decrypted with
  the matched client's own key (buf[25]/buf[26] for ECM/0-type messages, or buf[5]/buf[6] for cacheex/0x80-0x81
  messages, up to 65535). That length was used unchecked in two ways: (a) in cs378x_msg_peek() and
  cs378x_connect_cli(), to size a *second* recv() call that writes straight into a 1024/2048-byte stack buffer
  (recv(handle, buf+len, newlen-len, ...)) -- a remote stack buffer WRITE overflow of up to ~63KB past a 1-2KB
  stack array; (b) in the CAMD_ECM_REQUEST / 0x80 / 0x81 handlers (srv-camd35.c, srv-cs378x.c), as the size of a
  memcpy() into a fixed 512-byte ecmdata[] stack buffer (memcpy(ecmdata, buf+24, ecmlen)) -- a WRITE overflow of
  up to ~3.5KB past that buffer; and in srv-camd35.c's CRC check, as the size of a crc32() read starting at
  buf+24 -- an OOB READ past the 1024-byte buffer. All four are reachable without the client's password: once a
  connection is matched to a configured client purely by a 4-byte CRC32 of the username (ucrc), any ciphertext
  the attacker sends "decrypts" to effectively unpredictable bytes under the real per-client AES key, and roughly
  3 in 4 such attempts (for the 0-type/ECM branch) or any attempt landing on the cacheex/0x80-0x81 branch (full
  16-bit length, no masking) produce a length that overflows these fixed buffers.
- Fixed by clamping every one of these lengths, right where each is computed, to the real bound of the buffer it
  will be read from or written into: cs378x_msg_peek() and cs378x_connect_cli() cap newlen to the caller's actual
  buffer size (2048 / 1024) before the second recv(); all four ecmlen sites (2x srv-cs378x.c, 1x srv-camd35.c) cap
  to sizeof(ecmdata)=512; srv-camd35.c's CRC-check length is also capped to what was actually received (received-24).
  No protocol behavior changes for any legitimately-sized message.
- Verified: full GUI regression suite 107/107, clean compile and clean start under ASan/UBSan. No live camd35/
  cs378x exploit was built or sent against the service; found and fixed purely from source review, confirmed by
  the exact buffer-size arithmetic (declared buffer sizes vs. the maximum value each length field can take).

## v1.70.3 (2026-11-22)
- Security fix: the newcamd/mgcamd login handler read the username with strlen((char*)buf+3), but buf is a
  never-zeroed stack buffer and the receive function only writes as few as 3 bytes for the shortest possible
  login message. strlen() could then run past the written data into uninitialized stack memory. Reachable
  pre-auth by any client that completes the (standard, unauthenticated) newcamd handshake, on the newcamd,
  mgcamd and the mgcamd-status-connector code paths (3 call sites, same root cause).
- Fixed at the source: cs_message_receive()/cs_msg_peek() in msg-newcamd.c now NUL-terminate the buffer right
  after the bytes they actually received, so every caller always gets a valid, bounded C string.
- Verified: full GUI regression suite 107/107, clean start under ASan/UBSan. No live newcamd/mgcamd exploit was
  built or run against the service; the mechanism was confirmed with an isolated, offline reproduction of the
  exact buffer-size/write pattern.
## v1.70.2 (2026-11-22)
- Security fix: parse_http_request() read up to sizeof(buffer) bytes into a 2048-byte stack buffer and then wrote
  buffer[size]=0; an exact 2048-byte request (or growing to exactly 2048 across reads) wrote one byte past the
  buffer. Reachable pre-auth on the HTTP port. Found with a source build under AddressSanitizer + targeted fuzzing;
  fixed by reading at most sizeof(buffer)-1 bytes. Confirmed with ASan (0 reports after the fix) and the full GUI
  regression suite (107/107).
- Fix: dynbuf_realloc() could call memcpy() with a NULL source pointer (0 bytes) on a never-initialized dyn_buffer,
  undefined behaviour per the C standard even though harmless in practice. Found with UBSan; now guarded.
## v1.70.1 (2026-11-22)
- Fix: /iptables page threw "setautorefresh is not defined" (the auto-refresh script was never emitted); the page now
  supports ?action=div like the other pages, so the partial refresh works.
- Fix: /favicon.ico (and any unknown URL) closed the connection with no reply (net::ERR_EMPTY_RESPONSE in the browser).
  /favicon.ico now serves the menu logo as SVG, unknown URLs return a proper 404.
- README.md and configs_exemplos/profiles.cfg: repaired mojibake (broken dashes, arrows, ellipses and accents).
- deploy.ps1: the final check now tests real pages (/dashboard, /profiles, /iptables, /favicon.ico), prints each HTTP code and fails the deploy if one is not OK. It used the removed /packages page.
- ops/: VPS scripts versioned without credentials; ip2country daily update documented.
## v1.70.0 (2026-11-22)
- GUI redesigned (F3 "Atlas" graphite theme): side menu with brand header, dashboard status band
  (servers connected, clients, ECM answered, memory), pill buttons, darker data panels, light/dark themes.
- Whole GUI is now English only (labels, tooltips, confirmations, editor and upload messages).
- Line editor popup: styled by CSS, closes with Esc, focus moves into the popup and returns on close,
  DesKey row now toggles when the protocol changes.
- Clients menu opens by itself on a client page; footer shows the real version.
- Embedded CSS/JS are regenerated from gui_embedded_style.css / gui_embedded_customjs.js with
  tools_generate_httpstyle.py. Configs/multics.css now matches the embedded stylesheet and
  FILE STYLESHEET is commented out in Configs/multics.cfg (an old external multics.css hides the new theme).
- Dashboard compacted: the per-profile tiles are replaced by one Profiles panel (top 8 by ECM traffic, bar + OK/ECM,
  link to /profiles) and a single Traffic panel. Side menu column now spans the full page height.
- Configs/profiles.cfg trimmed to the MEO and NOS profiles only.
## v1.63.5 (2026-11-21)
- **Fix - GUI nao passava do login atras do nginx**: o parser HTTP so guardava os 20 primeiros cabecalhos (MAXHEADERS). Atras de um reverse proxy entram mais cabecalhos (X-Real-IP, X-Forwarded-*, Connection) e, no redirect pos-login, o browser acrescenta Referer/Origin; o Cookie e enviado no fim, ficava fora dos 20, a sessao nao era lida e voltava-se ao login. Os cabecalhos passam a 32 (MAXREQHEADERS) com nome de 48 B e a copia e agora limitada (antes uf2str nao tinha limite). O array e compacto para nao aumentar a stack da thread.
- Mitigacao imediata em producao: nginx deixou de acrescentar X-Real-IP/X-Forwarded-* e passou a enviar Connection: ".

## v1.63.4 (2026-10-24, produção)
- **Filtro de paridade NOS 1802** (regra dos 1785 pares clean: byte0=80 → metade0 muda / byte0=81 → metade1 muda) implementado mas **desativado** — as fontes atuais servem a metade errada com frequência (lag de 1 período) e rejeitar esfomeia a box (34% entregas). Documentado no estudo ECM→CW.
- Estado final do consenso corrigido (v1.63.2) em produção.

## v1.63.2 (2026-10-22)
- **Fix crítico — consenso invertido**: `!dcwcmp16` contava as fontes que DISCORDAM; a NOS ficava permanentemente negra com CWs corretas. Corrigido.

## v1.63.1 (2026-10-21)
- RE: dump do ECM completo no rawlog (multics-raw.log) para o estudo dos campos vs CW.

## v1.63 (2026-10-19)
- **SHEEP DETECTOR (detector da ovelha)**: mede a divergencia de CW por fonte vs a maioria (janela de 3s, mesma chave ECM). A fonte que diverge consistentemente (>=80% em >=20 comparacoes) e anunciada no log como CANDIDATA A ARBITRO - e a maquina universal que detecta a fonte de rotacao verdadeira dentro de um saco marcado, sem intervencao manual. Coluna Truth mostra SHEEP n/n (%) com marcador verde.
- VERSION_STR 1.63.

## v1.62.1 (2026-10-18)
- **Fix pagina Servers**: XML do AJAX com c15 (coluna Truth) e mapeamento correcto no JS (td13<-c15, td14<-c13) - os botoes de accao deixaram de duplicar na coluna Truth durante o refresh; colspan dos separadores 14->15; coluna Hop's com largura fixa (80px) e badge com nowrap.
- Estudo do veneno 1802 documentado (estudos 1-5): a metade fixa e o zipper sao a estrutura normal da CW Nagra (a MEO tem o mesmo padrao e abre); o veneno das fontes marcadas nao e atraso, cadencia nem desfasamento - sao CWs validas de outro dominio (incorrigivel no rele; o consenso/arbitro protegem).

## v1.62 (2026-10-16)
- **Dashboard**: tiles de Uptime moderno, saude por perfil (MEO/NOS com cor do OK%), Referee (ask/match/disagree sem host); widgets Fair-Share (validacoes do arbitro por cliente no ultimo minuto) e Protecções (CW Infected, Rejected, Disagree/Discard by Referee, Verdict OK/BLACK); contadores globais de arbitro/filtros/verdict.
- **Coluna Truth na pagina Servers**: % de CWs validas por fonte (ecmok vs ecmok+cwbad), contadores RAW/WRAP (veneno de periodo), nodeid e Resp. media - a agulha do veneno no palheiro.
- **VERDICT end-to-end**: anel de CWs servidas + endpoint /verdict (caid/prov/sid/cw/ok) - a box reporta o resultado real (abriu/preto) e a fonte que serviu a CW ma e marcada (dcw_badmark + log multics-verdict.log). Agente externo na box (grab luminancia + .cwl).
- Perfil [DEFAULT] deixa de contar como perfil; Packages Info com Caid|Ident Provider|Actions e Options/Filters em pilha; fix parser de query (9 params -> MAXHEADERS-1).

## v1.61 (2026-10-14)
- **Editor popup /editline** (form com protocolo/host/porta/user/pass/deskey/options; SAVE reconstrói a linha em Servers.cfg/Clients.cfg). Botões "Edit" nas paginas de servers e clients.
- **Fair-share do arbitro**: DCW ARBITER MAX por perfil (teto de validacoes por cliente na janela) - evita que um cliente monopole o arbitro.
- Deadlines 5500/5500 + retry de validacao quando o arbitro esta ocupado (fix da contenção multi-cliente).


- **Editor linha-a-linha** (pagina Editor): para `Servers.cfg` e `Clients.cfg` - cada linha ativa aparece como linha editavel com botoes ON/OFF (toggle do comentario - respeito total pelo mecanismo disabled da v1.54), SAVE (edicao inline via base64), DEL e ADD. Tudo com reload de config imediato (sem restart).
- Versao GUI/relatorio alinhada (VERSION_STR 1.60).

## v1.55.5 (2026-10-11)
- **ARBITER PRE-ASK**: o reader arbiter e perguntado EM PARALELO com a rotacao no inicio do ECM. Como e rapido (~300ms) e e a fonte da verdade, a entrega ao cliente cai de ~3s para ~300ms - a latencia alta era o gatilho do wedge do descrambler 1802 na box (freezes de 1-frame).
- **SILENT NOK desligavel por perfil** (recomendado NO no 1802): o NOK adiado fazia a box repetir o pedido e o cache responder com CW velha - dessincronizava o descrambler.
- Estudo completo documentado: veneno de periodo (CWs checksum-validas de outros criptoperiodos), filtros por comparacao #SUM, protocolo ponta-a-ponta.

## v1.55.4
- **ENABLE CACHE: NO no perfil 1802**: o cache servia CWs de periodos anteriores em re-requests do cliente (resposta 0ms) - causa raiz de freezes residuais.
- DCW TIMEOUT mais generoso no NOS (2500) para o ciclo pergunta+validacao.

## v1.55.3
- Arbitro sem resposta em 2.5s: o candidato NAO validado e DESCARTADO (marca a fonte) em vez de entregue - nunca entregar CWs nao confirmadas (um par errado dessincroniza a box).

## v1.55.2
- **STUCK RECOVERY**: busy preso de cliente (ECM orfao) > 10s -> reset automatico e aceite do pedido novo (auto-recuperacao; fim dos blackouts prolongados).
- Fix checktime=0 ("nunca re-verificar") no ramo de timeout do arbitro - o ECM ficava orfao para sempre e o busy do cliente nunca limpava.
- Stash re-entrado salta os filtros (falso positivo da meia carregada contra a propria cadeia).

## v1.55.1
- **DCW AGREE**: consenso - a mesma CW tem de vir de N fontes (`DCW AGREE: 2`); com arbitro ativo o arbitro e o consenso.
- **SERVER ASK**: fan-out - perguntar a N servers em paralelo por ECM (`SERVER ASK: 3`).

## v1.55
- **DCW HALFROTATE / HALFREPEAT TTL**: filtro de rotacao CW0/CW1 por canal+FONTE - rejeita pares sem meia em comum com o anterior e meias repetidas (o fingerprint das CWs envenenadas de periodo).
- **DCW ARBITER**: reader de confianca (`arbiter=yes` na linha, fora da rotacao normal) valida a CW de cada fonte; `DCW ARBITER SERVE: YES` entrega a CW do arbitro em discordancia; `DCW ARBITER RATE` limita a % de ECMs validados (protecao do cartao do arbitro). Degradacao graciosa quando o arbitro desaparece.
- Fonte do problema NOS 1802 identificada e neutralizada: cards marcados do saco devolvem CWs de criptoperiodos errados (checksum valido); o arbitro (fonte limpa) corrige.

## v1.54
- **Fix OFF/ON da GUI**: linhas comentadas (`#N:`/`#C:`/...) sao lidas como readers DISABLED (FLAG_DISABLE) - o OFF ja nao faz a linha desaparecer e o ON volta sempre. Exemplos de documentacao (`#    N: ...`) continuam ignorados.

## v1.53
- **Fix passwords com '#"**: o '#' so e comentario no inicio da linha ou apos espaco/tab - linhas C: com passwords a conter '#' (ex: `FiL#nW0$A9u0B`) passam a ser lidas corretamente.

## v1.52
- Fix badident (provider do ECM mal identificado), re-poll curto (300ms) quando as fontes estao ocupadas, fix da race timeout-vs-rotacao (o 2o/3o server nunca era perguntado), CAK7 INVERSE.

## v1.51
- Seccoes find-or-create (re-declarar [perfil] noutro ficheiro reutiliza o existente), grupo de apresentacao na GUI por reader, values de budget separados.

## v1.50
- **BUDGET ASK**: teto de pedidos/s por reader (anti rate-limit upstream por pedido, nao so por CW).
## v1.48 (2026-10-03)
- **DEADCHAN**: canais mortos (sem resposta ha N minutos) deixam de martelar os readers - NOK rapido ao cliente + 1 probe de recuperacao a cada RETRY segundos. Config: `DCW DEADCHAN: YES`, `DCW DEADCHAN MINTIME: 5`, `DCW DEADCHAN RETRY: 60`. Lista na pagina CW Monitoring.
- **PACING**: intervalo minimo entre pedidos do MESMO canal ao MESMO reader (`DCW PACING: 2000`) - anti rate-limit upstream (1 pedido por janela em vez do storm de retries).
- **VERDICT**: endpoint HTTP `/verdict` + anel das ultimas 128 CWs servidas - a box reporta se a CW abriu (OK) ou ficou preta (BLACK); BLACK marca a fonte exacta (dcw_badmark) e regista em `/var/log/multics-verdict.log`. Agente de exemplo para boxes Enigma2: le /tmp/ecm.info + estado do decoder vmpeg1 e reporta de 8 em 8s.
- **RAWLOG**: `DCW RAWLOG: YES` captura a CW crua PRE-transformacao em `/var/log/multics-raw.log` com classificacao RAW/WRAP pelos checksums de quad + detector de periodos de graca 1802 (eventos GRACE ON/OFF no dashboard).
- **Oráculo CWPK (telnet)**: comando `CWPKTEST <32hex>` testa uma chave candidata (3DES-EDE, 4 variantes de layout) contra as CWs 1802 WRAP capturadas - validacao instantanea quando aparecer um candidato (dump/leak/EMM).
- **CAK7 INVERSE**: `DCW CAK7 INVERSE: YES` usa a permutacao inversa (para fontes que entregam a CW ja icam-scrambled nos premium).
- **Divergencia entre readers**: contador por reader de CWs diferentes da aceite (coluna Div na CW Monitoring) - deteta readers a produzir CW errada.
- **Nanos por canal**: seccao na CW Monitoring com os nanos (ECM[2]) vistos por canal - deteta streams especiais (ex.: sport com nano 67 vs 66 nos basicos).
- **PROVID REWRITE**: opcao de reader `providrewrite=YES` reescreve o provid para 0 nos pedidos (rota do cartao real em servers que encaminham o ident para cacheex).
- Fix: probe de recuperacao do DEADCHAN (antes o canal marcado nunca voltava); TESTCHANNEL ja existia na GUI (pagina /testchannel).

## v1.28 (2026-09-14)
- **Novo tema "Stats Tiles"** (dark + light mode) com **menu lateral** (sidebar fixa): as 14 páginas ficam em coluna à esquerda com badges; dashboard com **tiles de números grandes** (Uptime, Servers, Clients, ECM Totais, RAM, Softcam) e barras de carga.
- **Tabela de Servers**: coluna EcmTime removida; Cards continua na última coluna com **chips coloridos por CAID:PROVID** (ident + nome do provider MEO/NOS/etc).
- **ON/OFF**: interruptores visuais (pills verdes/vermelhas) em todas as páginas de estado.
- **Drop-downs e uploads** restilizados (setas próprias, file-selector button, focus rings).
- **DEBUG**: log com fundo de consola e realce das linhas; botões DBG/INF uniformizados.
- Correção do xmlupdateRow (linhas com nº variável de colunas); cache-bust do CSS v=1128; título/footer v1.28.

# Changelog - MultiCS r1000 by Sharillas
Todas as alterações notáveis desde a v1.20, por versão.

## v1.26 (2026-08-31)
- **Estrutura de ficheiros PT**: `profiles.cfg` (perfis + users newcamd), `servidores.cfg` (N:/C:/L: + cache + cacheex), `clientes_cccam/mgcamd/cs378x/camd35/cache.cfg` (users por protocolo). Compatibilidade total: manter tudo no `multics.cfg` continua a funcionar; o parser é o mesmo (INCLUDEs).
- **CWPK LEARNING**: o filtro DCW FILTER aprende novas regras em runtime — 5 CWs más iguais do mesmo IP → regra EXACT adicionada (cap 16, FIFO, log + evento no dashboard). `DCW FILTER LEARN: YES` por perfil.
- **Eventos recentes no Dashboard**: secção "Proteções & Eventos" com uptime do processo, ECMs totais (OK/NOK), regras CWPK aprendidas e o anel dos últimos eventos (AUTO ativado, FAILBAN, ANTICASCADE, LEARNING).
- **Packages**: coluna "Filtros" com badges por pacote (CAK7, CWPK AUTO/ATIVO/DROP/LOGONLY, LEARN, ECM FILTER).
- **Editor Configs**: CCcam.providers editável; dropdown uniforme com caminho real; multics.css e ip2country.csv fora do editor (geridos à parte).
- **Cabeçalhos para leigos** em todos os ficheiros de config (produção + exemplos): como fazer N/C/L-line, F-line, MG user, CACHE, CACHEEX, USER newcamd, BISS, providers, channelinfo. Gerador do channelinfo preserva o cabeçalho.
- **install.sh sem git clone**: copia só bin + configs e no fim mostra guia completo (onde estão os ficheiros, como funciona, primeiros passos).
- Fix: ordem dos INCLUDEs no multics.cfg (F-lines/HTTP/TELNET dependem de ordem) — documentado no ficheiro.

## v1.25 (2026-08-31)
- **DCW FILTER (CWPK)**: blacklist de CWs de cartões marcados (35 valores extraídos por reverse engineering do MultiCS r120); modos `AUTO` (ativa-se sozinho no 1º hit), `LOGONLY`, `DROP`; regras `EXACT` (até 8 por regra), `MASK`, `ALLEQUAL`.
- **ECM FILTER (rule engine)**: `PREFIX`, `LEN` multi, `BYTE <pos> INMASK <mask64>` por perfil, modo DROP/LOGONLY — a validação iCAM é config (`BYTE 21 INMASK 1300010012`).
- **FAILBAN** por protocolo (CCCAM/NEWCAMD/MGCAMD/CAMD35/CS378X/CACHE) com BANTIME → ipblock.
- **ANTICASCADE**: anti-reshare por zapping excessivo (MAXZAP/WINDOW/BANTIME).
- **ECMRATELIMIT**: proteção do cartão físico (SIDTIME/MAXECM por perfil).
- **DCW CAK7 (CAK7 Merlin)**: transformação iCAM da CW (permutação de bits + checksum dos quads, algoritmo extraído do r120) para Sky DE 098D/MEO/NOS + perfil [SkyDE-098D] de exemplo.

## v1.24 (2026-08-31)
- **SILENT NOK adiado** (2.5s) em todos os protocolos: falhas respondem antes do timeout da box do cliente — elimina reconexões e storms de retries.
- GUI: fix do footer do dashboard com versão hardcoded; DBG/OFF/ON dos perfis corrigidos; status line HTTP nos divs AJAX (autorefresh e botões a funcionar em todos os browsers).
- 098D: suporte a cartões hop1/hop2 no fluxo; limpeza de avisos (MSG_ECHO 0x02 do reader → debug).

## v1.23 (2026-08-30)
- Login guard anti brute-force (5 falhas/30s por IP) + sessões persistentes + "Terminar todas as sessões" + invalidação ao mudar password.
- ECM Dedup (1 pedido único por ECM em voo) com stats na página Servers.
- NOK cache por reader+canal (~8s) — menos tráfego NOK em zappings.
- ip2country diário (iplocate) com bandeiras; ferramenta update_ip2country.py.
- Pacote dos 3 satélites (30W/13E/19.2E) em profiles/providers/channelinfo/lite.
- Filtro rigoroso de idents (ACCEPT NULL PROVIDER NO por defeito), nomes de providers corrigidos, parser SRVID com idents a 6 dígitos.
- GUI: coluna Cards agrupada por perfil, responsividade mobile, fuzz_http.sh, crash-report com core dumps, CI GitHub Actions (build-musl), deploy.ps1.

## v1.20-v1.22
- Base r1000: GUI moderna, Softcam BISS/CW, SKIPCWC, CWC, NAGRA protection, anti-fake XOR 0xF0/nano e0, Health scoring, Fallback cross-protocol, Timing budget, BUILD LITE, upload validado com rollback, editor AJAX, CCcam 3.0.1 (RSA+AES-GCM em C puro), DEDUP, assinaturas de protocolo.
