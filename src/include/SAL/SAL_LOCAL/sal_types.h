typedef enum {
     TRANSACT_LIVE,
     TRANSACT_COMPLETED,
     TRANSACT_ABORTED,
     TRANSACT_FAILED,
     TRANSACT_PYRRHIC_VICTORY
} state_trans_status_t;

typedef enum {
     ERROR_SOURCE_SAL,
     ERROR_SOURCE_FSAL,
     ERROR_SOURCE_HASHTABLE
} state_trans_errsource_t;

typedef struct state_share_trans__ {
     state_trans_status_t status;
     state_trans_errsource_t errsource;
     uint32_t errcode;
     struct state__* share_state;
     open_owner_t owner;
}
