# Guião de Demonstração: Concurrent HTTP Server

Este documento serve como guião para a apresentação e demonstração das funcionalidades do projeto **Concurrent HTTP Server**.

## 1. Preparação do Ambiente

Antes de iniciar a demo, garanta que o ambiente está limpo e compilado.

**Passo 1: Limpar e Compilar**
Abra um terminal na raiz do projeto e execute:

```bash
make clean
make
```

Isto garante que não há ficheiros antigos ou logs residuais e que o binário `server` está atualizado.

## 2. Execução do Servidor

Mostre como iniciar o servidor.

**Passo 2: Iniciar o Servidor**
No terminal, execute:

```bash
./server
```
*(Opcional: Use `./server -v` para modo verboso se quiser mostrar logs detalhados no terminal)*

Deverá ver uma mensagem indicando que o servidor iniciou (ex: "Server listening on port 8080").

## 3. Página Principal (Index)

Demonstre a capacidade de servir ficheiros estáticos básicos.

**Passo 3: Aceder ao Index**
Abra o browser e vá a:
`http://localhost:8080/index.html`

*   **Destaque:** O carregamento da página com CSS e Imagens.
*   **Ação:** Clique no botão "Ping Server (JS Test)" na secção "Interactivity Test".
    *   Isto demonstra que o servidor responde a pedidos dinâmicos/JS corretamente.
*   **Ação:** Mostre os links para a documentação (Design Document, Report, User Manual) para provar que o servidor serve PDFs corretamente.

## 4. Dashboard e Estatísticas

Mostre a funcionalidade de monitorização em tempo real.

**Passo 4: Aceder ao Dashboard**
Abra um novo separador (ou janela) e vá a:
`http://localhost:8080/dashboard.html`

*   **Explique:**
    *   **Active Connections:** Número de clientes ligados no momento.
    *   **Total Requests:** Contador global de pedidos atendidos.
    *   **Data Transferred:** Volume de dados enviados.
    *   **Avg Response Time:** Latência média.
    *   **Gráfico:** Mostra os pedidos por segundo em tempo real.

Mantenha esta janela visível para o próximo passo.

## 5. Testes de Carga e Concorrência

Demonstre a robustez do servidor e a atualização do dashboard em tempo real.

**Passo 5: Gerar Tráfego**
Abra um **segundo terminal**. Vamos usar o `ab` (Apache Benchmark) ou o script de teste para gerar carga.

Execute o seguinte comando para simular 10.000 pedidos com 100 conexões concorrentes:

```bash
ab -k -n 10000 -c 100 http://localhost:8080/index.html
```

**Enquanto o teste corre:**
*   Volte rapidamente ao browser com o **Dashboard**.
*   **Observe:** O gráfico a subir, o "Total Requests" a incrementar rapidamente e o "Data Transferred" a aumentar.
*   Isto prova que o servidor lida com concorrência e que as estatísticas (memória partilhada) funcionam corretamente.

## 6. Gestão de Erros

Demonstre que o servidor lida corretamente com diferentes tipos de erros, retornando as páginas personalizadas.

**Passo 6.1: Erro 404 (Not Found)**
*   **Terminal (Técnico):**
    ```bash
    curl -v http://localhost:8080/ficheiro_inexistente.html
    ```
    *Observe o código 404 e o HTML da página de erro no terminal.*

*   **Browser (Visual):**
    Abra um novo separador e tente aceder a:
    `http://localhost:8080/ficheiro_inexistente.html`
    *Verifique que aparece a página de erro "404 Not Found" com o design personalizado.*

**Passo 6.2: Erro 403 (Forbidden)**
*   **Terminal (Técnico):**
    ```bash
    curl -v --path-as-is http://localhost:8080/../../etc/passwd
    ```
    *Observe o código 403.*

*   **Browser (Visual):**
    Para ver o design da página de erro 403, aceda diretamente a:
    `http://localhost:8080/errors/403.html`
    *(Nota: O browser normaliza o URL, impedindo o ataque real pela barra de endereços, por isso visualizamos a página diretamente)*

**Passo 6.3: Erro 405 (Method Not Allowed)**
*   **Terminal (Técnico):**
    ```bash
    curl -v -X POST http://localhost:8080/index.html
    ```
    *Observe o código 405.*

*   **Browser (Visual):**
    Para ver o design da página de erro 405, aceda diretamente a:
    `http://localhost:8080/errors/405.html`
    *(Nota: A barra de endereços faz apenas pedidos GET, por isso visualizamos a página diretamente)*

**Passo 6.4: Erro 400 (Bad Request)**
*   **Terminal (Técnico):**
    ```bash
    printf "GARBAGE_REQUEST\r\n\r\n" | nc localhost 8080
    ```
    *Observe o código 400.*

*   **Browser (Visual):**
    Para ver o design da página de erro 400, aceda diretamente a:
    `http://localhost:8080/errors/400.html`

**Passo 6.5: Erro 503 (Service Unavailable)**
Para simular sobrecarga, vamos limitar artificialmente o servidor e bombardeá-lo.

1.  Pare o servidor atual.
2.  Inicie com recursos mínimos (fila de 1, 1 thread):
    ```bash
    HTTP_QUEUE=1 HTTP_THREADS=1 ./server
    ```
3.  Num outro terminal, gere carga excessiva:
    ```bash
    ab -n 1000 -c 50 http://localhost:8080/index.html
    ```
*   **Resultado (Terminal):** Alguns pedidos falharão com 503.

*   **Browser (Visual):**
    Para ver o design da página de erro 503, aceda diretamente a:
    `http://localhost:8080/errors/503.html`

*(Nota: O erro 500 Internal Server Error ocorre apenas em falhas críticas de sistema como falta de memória, difícil de reproduzir em demo normal)*

**Passo 6.5: Erro 503 (Service Unavailable)**
Para simular sobrecarga, vamos limitar artificialmente o servidor e bombardeá-lo.

1.  Pare o servidor atual.
2.  Inicie com recursos mínimos (fila de 1, 1 thread):
    ```bash
    HTTP_QUEUE=1 HTTP_THREADS=1 ./server
    ```
3.  Num outro terminal, gere carga excessiva:
    ```bash
    ab -n 1000 -c 50 http://localhost:8080/index.html
    ```
*   **Resultado (Terminal):** Alguns pedidos falharão com 503.

*   **Browser (Visual):**
    Para ver o design da página de erro 503, aceda diretamente a:
    `http://localhost:8080/errors/503.html`

## 7. Testes Automatizados

Para finalizar, mostre a suite de testes completa a passar.

**Passo 7: Correr a Suite de Testes**
Pare o servidor atual (Ctrl+C no primeiro terminal) ou use um novo terminal.
Execute o comando de teste do projeto:

```bash
make test
```
*(Isto irá correr o script `tests/test_load.sh`)*

**O que vai acontecer:**
1.  O script compila o servidor e os clientes de teste.
2.  Inicia o servidor em background.
3.  Executa testes funcionais (curl) para verificar 200 OK, Content-Types (CSS, PDF, Imagens) e 404.
4.  Executa testes de concorrência com `ab`.
5.  Executa o `test_concurrent` (cliente C personalizado) para verificar sincronização de threads.
6.  Executa um teste de stress.

**Resultado:** Todos os testes devem apresentar um "visto" verde ou mensagem de sucesso.

## 8. Conclusão

Resuma o que foi demonstrado:
1.  Servidor estável e performante.
2.  Capacidade de servir múltiplos tipos de conteúdo (HTML, CSS, JS, PDF).
3.  Monitorização em tempo real via Dashboard.
4.  Tratamento correto de erros.
5.  Validação através de uma suite de testes robusta.
