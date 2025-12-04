// Inês Batista, 124877
// Maria Quinteiro, 124996

#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // Para time_t, simulando verificação de modificação

// Função auxiliar simples para simular o hashing do path para um índice
static size_t hash_path(const char *path) {
    unsigned long hash = 5381; // Valor inicial 
    int c;
    while ((c = *path++)) {
        hash = ((hash << 5) + hash) + c; // Algoritmo DJB2
    }
    return hash % MAX_CACHE_ENTRIES; // Mapeia o hash para um índice dentro dos limites
}



int cache_init(file_cache_t *cache) {
    if (!cache) return -1;
    
    // Inicializa o Reader-Writer Lock (PONTO CRÍTICO DO DIA 11)
    if (pthread_rwlock_init(&cache->rwlock, NULL) != 0) {
        perror("ERRO: Falha ao inicializar pthread_rwlock na cache");
        return -1;
    }

    // Inicializa as entradas do array para um estado limpo
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        cache->entries[i].is_valid = 0;
        cache->entries[i].content = NULL;
        cache->entries[i].size = 0;
    }
    cache->total_size = 0; // O tamanho inicial da cache é zero
    
    printf("[Cache] Cache inicializada com %d entradas.\n", MAX_CACHE_ENTRIES);
    return 0;
}



const char* cache_get(file_cache_t *cache, const char *path, size_t *size) {
    const char *content = NULL;
    size_t index;
    
    // 1. ADQUIRIR READ LOCK (rdlock)
    // Permite que N threads leiam simultaneamente (alta concorrência)
    pthread_rwlock_rdlock(&cache->rwlock);

    index = hash_path(path); // Calcular o índice na cache
    cache_entry_t *entry = &cache->entries[index];

    // Verificar se o slot tem dados e se a chave corresponde (simples tratamento de colisão)
    if (entry->is_valid && strcmp(entry->path, path) == 0) {
        // Encontrado: Copiar dados e retornar
        *size = entry->size;
        content = entry->content; // Retorna o ponteiro para o conteúdo
    } 
    
    // 2. LIBERTAR READ LOCK
    // Outras threads leitoras ou escritoras podem agora prosseguir
    pthread_rwlock_unlock(&cache->rwlock);

    return content;
}



int cache_put(file_cache_t *cache, const char *path, const char *content, size_t size) {
    int result = -1;
    size_t index;
    
    // Verificação de tamanho para prevenir alocações excessivas
    if (size > MAX_FILE_SIZE) {
        fprintf(stderr, "[Cache] AVISO: Ficheiro muito grande para cache (%zu bytes).\n", size);
        return -1;
    }
    
    // 1. ADQUIRIR WRITE LOCK (wrlock)
    // BLOQUEIA TODOS os Leitores e outros Escritores (acesso exclusivo)
    pthread_rwlock_wrlock(&cache->rwlock);

    index = hash_path(path);
    cache_entry_t *entry = &cache->entries[index];

    // Limpar entrada antiga (se existir)
    if (entry->is_valid) {
        if (entry->content) {
            cache->total_size -= entry->size; // Atualiza o tamanho total
            free(entry->content);             // Liberta a memória antiga
            entry->content = NULL;
        }
    }

    // Alocar e copiar novo conteúdo
    entry->content = (char *)malloc(size);
    if (entry->content) {
        memcpy(entry->content, content, size); // Copia o conteúdo fornecido
        
        // Preencher a entrada com novos metadados
        strncpy(entry->path, path, MAX_PATH_LEN - 1);
        entry->path[MAX_PATH_LEN - 1] = '\0';
        entry->size = size;
        entry->last_modified = time(NULL); // Define o tempo de criação/atualização
        entry->is_valid = 1;               // Marca a entrada como válida
        cache->total_size += size;         // Atualiza o tamanho total
        
        result = 0;
    } else {
        fprintf(stderr, "[Cache] ERRO: Falha ao alocar memória para cache.\n");
    }
    
    // 2. LIBERTAR WRITE LOCK
    // O acesso normal (leitores) é retomado
    pthread_rwlock_unlock(&cache->rwlock);

    return result;
}


void cache_destroy(file_cache_t *cache) {
    if (!cache) return;
    
    // Lock de escrita para garantir que ninguém acede durante a destruição
    pthread_rwlock_wrlock(&cache->rwlock); 
    
    // Limpar todas as entradas alocadas
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache->entries[i].is_valid && cache->entries[i].content) {
            free(cache->entries[i].content);
            cache->entries[i].content = NULL;
        }
        cache->entries[i].is_valid = 0;
    }
    
    cache->total_size = 0;
    
    // Destruir o Reader-Writer Lock
    pthread_rwlock_unlock(&cache->rwlock);
    pthread_rwlock_destroy(&cache->rwlock);
    
    printf("[Cache] Cache destruída e recursos libertados.\n");
}