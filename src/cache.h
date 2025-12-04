// Inês Batista, 124877
// Maria Quinteiro, 124996

#ifndef CACHE_H
#define CACHE_H

#include <pthread.h> // Necessário para pthread_rwlock_t
#include <stddef.h>  // Para size_t

// Tamanho máximo da cache (número de ficheiros que pode guardar)
#define MAX_CACHE_ENTRIES 128
// Tamanho máximo do nome do ficheiro (path)
#define MAX_PATH_LEN 256
// Tamanho máximo do conteúdo de um ficheiro (em bytes)
#define MAX_FILE_SIZE (1024 * 1024) // 1MB por ficheiro de exemplo




// Estrutura para uma única entrada de ficheiro na cache
typedef struct {
    char path[MAX_PATH_LEN];      // O caminho do ficheiro (chave)
    char *content;                // O conteúdo do ficheiro (alocado dinamicamente)
    size_t size;                  // O tamanho do conteúdo em bytes
    time_t last_modified;         // Tempo da última modificação do ficheiro original (para futura validação)
    int is_valid;                 // Flag: 1 se a entrada for válida, 0 se estiver vazia
} cache_entry_t;

// Estrutura principal da Cache
typedef struct {
    cache_entry_t entries[MAX_CACHE_ENTRIES]; // O array de entradas da cache
    size_t total_size;                        // Tamanho total ocupado na cache (opcional, para gestão)
    
    // Reader-Writer Lock: Permite concorrência de leitores (NOVIDADE DIA 11)
    pthread_rwlock_t rwlock;                  // O lock principal para proteger a estrutura
} file_cache_t;




/**
 * Inicializa a Cache de Ficheiros.
 * Deve ser chamada uma vez pelo Worker Main.
 * @param cache: Ponteiro para a estrutura da cache a inicializar.
 * @return 0 em caso de sucesso, -1 em caso de falha.
 */
int cache_init(file_cache_t *cache);

/**
 * Tenta obter um ficheiro da cache (Leitor).
 * Requer um Read Lock (rdlock).
 * @param cache: Ponteiro para a cache.
 * @param path: O caminho do ficheiro a procurar.
 * @param size: Ponteiro para onde guardar o tamanho do ficheiro (saída).
 * @return Ponteiro para o conteúdo do ficheiro na cache, ou NULL se não for encontrado.
 */
const char* cache_get(file_cache_t *cache, const char *path, size_t *size);

/**
 * Adiciona ou atualiza um ficheiro na cache (Escritor).
 * Requer um Write Lock (wrlock).
 * @param cache: Ponteiro para a cache.
 * @param path: O caminho do ficheiro.
 * @param content: O conteúdo a guardar (será copiado).
 * @param size: O tamanho do conteúdo.
 * @return 0 em caso de sucesso, -1 em caso de falha (ex: sem espaço).
 */
int cache_put(file_cache_t *cache, const char *path, const char *content, size_t size);

/**
 * Limpa todos os recursos da cache.
 * @param cache: Ponteiro para a cache.
 */
void cache_destroy(file_cache_t *cache);

#endif