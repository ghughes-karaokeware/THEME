CHPT_ABI_VERSION       EQUATE(00010000H)
CHPT_DATA_SIZE         EQUATE(4256)
CHPT_TEXT_CAPACITY     EQUATE(4096)

CHPT_RESULT_CANCEL     EQUATE(0)
CHPT_RESULT_OK         EQUATE(1)

CHPT_FLAG_DEBUG_LOG    EQUATE(00000001H)

CHPT_STATUS_OK         EQUATE(1)
CHPT_ERROR_ARGUMENT    EQUATE(-1)
CHPT_ERROR_VERSION     EQUATE(-2)
CHPT_ERROR_SIZE        EQUATE(-3)
CHPT_ERROR_BUFFER      EQUATE(-4)
CHPT_ERROR_ALREADY_OPEN EQUATE(-5)
CHPT_ERROR_WINDOW      EQUATE(-6)
CHPT_ERROR_ENCODING    EQUATE(-7)
CHPT_ERROR_LENGTH      EQUATE(-8)

CHPT_PROMO_DATA        GROUP,TYPE
Version                  ULONG
StructureSize            ULONG
Flags                    ULONG
BufferCapacity           ULONG
MaximumLength            ULONG
InstanceID               ULONG
Result                   LONG
InputLength              ULONG
OutputLength             ULONG
Text                     CSTRING(CHPT_TEXT_CAPACITY)
Reserved                 STRING(124)
                       END
