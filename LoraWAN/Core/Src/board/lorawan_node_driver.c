/*!
 * @file       lorawan_node_driver.c
 * @brief      lorawan_module_driver �����lierda LoRaWANģ�����������
 * @details    ���ļ�Ϊ�û��ṩһ����LoRaWANģ���׼�������������̡���ģ����ָ���,�Ĳ�����װ�ɺ����ӿڣ����û����ã��Ӷ����̲�Ʒ��������
 * @copyright  Revised BSD License, see section LICENSE. \ref LICENSE
 * @date       2019-07-23
 * @version    V1.1.6
 */

/*!
 * @defgroup  ICA-Driver LoRaWAN_ICA_Node_Driver API
 * @brief     ���ļ�Ϊ�û��ṩһ�����LoRaWAN_ICAģ��Ĵ���������
 * @{
 */
#include "lorawan_node_driver.h"
#include "common.h"

/** @defgroup ICA_Driver_Exported_Variables ICA_Driver_Variables
  * @brief ICA���������ṩ���û��ı���
  * @{
  */

/** ģ���Ƿ��ѳɹ����� */
bool node_join_successfully = false;

/** ȷ��֡����ʧ�ܴ��� */
uint8_t confirm_continue_failure_count = 0;

/** ���һ���������ݵ����� */
int8_t last_up_datarate = -1;

/** �ն˲���λ������¶�ģ�鸴λ���ź��� */
node_reset_single_t node_reset_signal;

extern DEVICE_MODE_T device_mode;

/**
  * @}
  */

static void transfer_node_data(uint8_t *buffer, uint8_t size);
static gpio_level_t node_gpio_read(node_gpio_t gpio);
static void node_gpio_set(node_gpio_t type, node_status_t value);
static bool transfer_configure_command(char *cmd);
static bool transfer_inquire_command(char *cmd, uint8_t *result);
static uint8_t handle_cmd_return_data(char *data, uint8_t start, uint8_t length);
static bool nodePinBusyStatusHolding( gpio_level_t gpio_level, uint32_t time );
static bool node_block_join(uint16_t time_second);
static void down_data_process(down_list_t **list_head);
static void free_down_info_list(down_list_t **list_head, free_level_t free_level);
static execution_status_t node_block_send(uint8_t *buffer, uint8_t size, down_list_t **list_head);
static uint32_t wait_as_respone(uint8_t *packet_end, uint8_t size, uint8_t packet_mark, uint8_t resend_num, down_list_t **list_head);
static void node_hard_reset(void);
static void commResultPrint(execution_status_t send_result);

/**
*  �����û����ݸ�ģ��
*@param 	buffer: Ҫ���͵���������
*@param		size: ���ݵĳ���
*/
static void transfer_node_data(uint8_t *buffer, uint8_t size)
{
    UART_TO_LRM_RECEIVE_FLAG = 0;
    UART_TO_MODULE_WRITE_DATA(buffer, size);
}


/**
*  ��ȡģ�����ŵĵ�ƽ
*@param 	gpio: ��ʾҪ��ȡ����STAT���Ż���BUSY����
*@param		d: Ҫ���ҵ��ַ���
*@return	��Ӧ���ŵĵ�ƽ
*/
static gpio_level_t node_gpio_read(node_gpio_t gpio)
{
    if(stat == gpio)
    {
        return (gpio_level_t)GET_STAT_LEVEL;
    }
    else if(busy == gpio)
    {
        return (gpio_level_t)GET_BUSY_LEVEL;
    }

    return unknow;
}


/**
*  ����ģ������ŵ�ƽ
*@param 	type: ��ʾ���õ���WAKE���Ż���STAT����
*@param 	value: ��ʾ���ö�Ӧ���ŵĸߵ͵�ƽ
*/
static void node_gpio_set(node_gpio_t type, node_status_t value)
{
    if(mode == type)
    {
        if(command == value)
        {
            if(GET_MODE_LEVEL == GPIO_PIN_RESET)
            {
                SET_MODE_HIGH;
                system_delay_ms(10);
            }
        }
        else if(transparent == value)
        {
            if(GET_MODE_LEVEL == GPIO_PIN_SET)
            {
                SET_MODE_LOW;
                system_delay_ms(10);
            }
        }
    }
    else if(wake == type)
    {
        if(wakeup == value)
        {
            if(GET_WAKE_LEVEL == GPIO_PIN_RESET)
            {
                SET_WAKE_HIGH;
                system_delay_ms(10);
            }
        }
        else if(sleep == value)
        {
            if(GET_WAKE_LEVEL == GPIO_PIN_SET)
            {
                SET_WAKE_LOW;
                system_delay_ms(10);
            }
        }
    }
}


/**
*  ִ��ģ������ָ��Դ�ģʽ�л�
*@param		cmd: ��Ҫִ�е�����ָ��
*@return	ָ��ִ�н��
*@retval		true:  ִ�гɹ�
*@retval		false: ִ��ʧ��
*/
static bool transfer_configure_command(char *cmd)
{
    uint8_t *result = NULL;
    uint16_t timeouts = 0;
    char tmp_cmd[255];

    node_gpio_set(wake, wakeup);
    node_gpio_set(mode, command);

    lower2upper_and_remove_spaces((uint8_t *)cmd, (uint8_t *)tmp_cmd);
    strcat(tmp_cmd, "\r\n");

    UART_TO_LRM_RECEIVE_FLAG = 0;
    memset(UART_TO_LRM_RECEIVE_BUFFER, 0, sizeof(UART_TO_LRM_RECEIVE_BUFFER));

    UART_TO_LRM_WRITE_STRING((uint8_t *)tmp_cmd);

    if(NULL != find_string((uint8_t *)tmp_cmd, (uint8_t *)"AT+SAVE"))
    {
        timeouts = 2000;
    }
    else
    {
        timeouts = 50;
    }

    timeout_start_flag = true;
    while(UART_TO_LRM_RECEIVE_FLAG == 0)
    {
        if(true == time_out_break_ms(timeouts))
        {
            break;
        }
    }

    UART_TO_LRM_RECEIVE_FLAG = 0;
    result = find_string(UART_TO_LRM_RECEIVE_BUFFER, (uint8_t *)"OK");

    if(NULL != find_string((uint8_t *)tmp_cmd, (uint8_t *)"AT+RESET") && NULL != result)
    {
        system_delay_ms(150);
    }

    if(NULL != find_string((uint8_t *)tmp_cmd, (uint8_t *)"AT+FACTORY") && NULL != result)
    {
        system_delay_ms(2000);
    }

    return result;
}

/**
*  ִ��ģ���ѯָ��Դ�ģʽ�л�
*@param		cmd: ��Ҫִ�еĲ�ѯָ��
*@return	result: ��ѯָ��ķ��ؽ��
*@return	ָ��ִ�н��
*@retval		true:  ִ�гɹ�
*@retval		false: ִ��ʧ��
*/
static bool transfer_inquire_command(char *cmd, uint8_t *result)
{
    uint8_t *response = NULL;
    uint8_t cmd_content[20] = {0}, i = 0;
    char tmp_cmd[255];

    node_gpio_set(wake, wakeup);
    node_gpio_set(mode, command);

    lower2upper_and_remove_spaces((uint8_t *)cmd, (uint8_t *)tmp_cmd);
    strcat(tmp_cmd, "\r\n");

    UART_TO_LRM_RECEIVE_FLAG = 0;
    memset(UART_TO_LRM_RECEIVE_BUFFER, 0, sizeof(UART_TO_LRM_RECEIVE_BUFFER));

    UART_TO_LRM_WRITE_STRING((uint8_t *)tmp_cmd);

    timeout_start_flag = true;
    while(UART_TO_LRM_RECEIVE_FLAG == 0)
    {
        if(true == time_out_break_ms(500))
        {
            break;
        }
    }
    UART_TO_LRM_RECEIVE_FLAG = 0;

    response = find_string(UART_TO_LRM_RECEIVE_BUFFER, (uint8_t *)"OK");
    if(response)
    {
        match_string((uint8_t *)cmd, (uint8_t *)"AT", (uint8_t *)"?", cmd_content);
        while(0 != cmd_content[++i]);
        cmd_content[i++] = ':';
        match_string(UART_TO_LRM_RECEIVE_BUFFER, cmd_content, (uint8_t *)"OK", result);
    }
    else
    {
        memcpy(result,UART_TO_LRM_RECEIVE_BUFFER,strlen((char*)UART_TO_LRM_RECEIVE_BUFFER));
    }
    return response;
}


/**
* �������ֲ�ѯָ��ķ���ֵ�����������ڲ����ã�
*/
static uint8_t handle_cmd_return_data(char *data, uint8_t start, uint8_t length)
{
    static uint8_t inquire_return_data[150];

    memset(inquire_return_data, 0, 150);
    transfer_inquire_command(data, inquire_return_data);

    return htoi((inquire_return_data + start), 2);
}


/**
*  ģ��BUSY����״̬����ʱ��
*@param		gpio_level�����ŵ�ƽ; time: ״̬���ֵĳ�ʱʱ��
*@return	ģ��BUSY����״̬�ڳ�ʱʱ�����Ƿ�ı�
*@retval		true:  BUSY���Ÿı䵱ǰ״̬
*@retval		false: BUSY���ű��ֵ�ǰ״̬
*/
static bool nodePinBusyStatusHolding( gpio_level_t gpio_level, uint32_t time )
{
    timeout_start_flag = true;
    while( gpio_level == node_gpio_read( busy ) )
    {
        if( true == time_out_break_ms( time ) )
        {
            return false;
        }
    }
    return true;
}


/**
 * @brief   ����ɨ��
 * @details ����������ʽ��������ɨ��
 * @param   [IN]time_second: ������ʱʱ�䣬��λ����
 * @return  ���������Ľ��
 * @retval		true:  �����ɹ�
 * @retval		false: ����ʧ��
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API����������ɨ��
 * @code
 * // ��λ�ڵ㲢����ɨ�裬��ʱʱ������Ϊ300�루��ʱʱ���ƽ�⹦���������ɹ����趨��
 * if(node_block_join(300) == true)
 * {
 *   // �ѳɹ�����
 * }
 * else
 * {
 *	 // ����ʧ��
 * }
 * @endcode
 */
static bool node_block_join(uint16_t time_second)
{
    gpio_level_t stat_level;
    gpio_level_t busy_level;

#ifdef DEBUG_LOG_LEVEL_1
    DEBUG_PRINTF("Start to join...\r\n");
#endif

    // 1.�л���͸��ģʽ��ʼ����
    node_gpio_set(wake, wakeup);
    node_gpio_set(mode, transparent);

    timeout_start_flag = true;
    do
    {
        // 2.ѭ����ѯSTAT��BUSY���ŵ�ƽ
        stat_level = node_gpio_read(stat);
        busy_level = node_gpio_read(busy);

        // 3.�����趨ʱ�����δ������ʱ����
        if(true == time_out_break_ms(time_second * 1000))
        {
            node_join_successfully = false;

            node_gpio_set(wake, sleep);

#ifdef DEBUG_LOG_LEVEL_1
            DEBUG_PRINTF("Join failure\r\n");
#endif
            return false;
        }

        if(device_mode == CMD_CONFIG_MODE)
        {
            node_join_successfully = false;
            return false;
        }

        /* ��ģ����־�������ȴ�������־ */
        if(UART_TO_LRM_RECEIVE_FLAG)
        {
            UART_TO_LRM_RECEIVE_FLAG = 0;
            usart2_send_data(UART_TO_LRM_RECEIVE_BUFFER,UART_TO_LRM_RECEIVE_LENGTH);
        }
    } while(high != stat_level || high != busy_level);

    timeout_start_flag = true;

    /* ����־�������ȴ����һ��������־ */
    while(0 == UART_TO_LRM_RECEIVE_FLAG)
    {
        if(true == time_out_break_ms(300))
        {
            break;
        }
    }
    if(UART_TO_LRM_RECEIVE_FLAG)
    {
        UART_TO_LRM_RECEIVE_FLAG = 0;
        usart2_send_data(UART_TO_LRM_RECEIVE_BUFFER,UART_TO_LRM_RECEIVE_LENGTH);
    }

#ifdef USE_NODE_STATUS
    UART_RECEIVE_FLAG = 0;

    timeout_start_flag = true;
    while(0 == UART_RECEIVE_FLAG && false == time_out_break_ms(50));

    if(1 == UART_RECEIVE_FLAG)
    {
        UART_RECEIVE_FLAG = 0;
        last_up_datarate = UART_RECEIVE_BUFFER[4];
    }
#endif

#ifdef DEBUG_LOG_LEVEL_1
    DEBUG_PRINTF("Join seccussfully\r\n");
#endif
    // 5.��δ��ʱ���������ģ�������ɹ�
    node_join_successfully = true;
    return true;
}




/**
* ���н������ݴ���
*@param		list_head: ָ��������Ϣ������ͷ��ָ��
*@return    ��
*/
static void down_data_process(down_list_t **list_head)
{
    down_list_t *p1 = NULL;
    down_list_t *p2 = NULL;
    uint16_t cnt = 0;

#ifdef USE_NODE_STATUS
    // �������������������㣬���ʾ�ⲻ�����һ�����а�������ҵ������
    if(UART_RECEIVE_LENGTH > 5 || high == node_gpio_read(busy))
    {
#endif
        if(NULL != *list_head)
        {
            p2 = *list_head;

            while(p2->next)
            {
                p2 = p2->next;
            }
        }

        p1 = (down_list_t *)malloc(sizeof(down_list_t));

        if(NULL != p1)
        {
#ifdef USE_NODE_STATUS
            p1->down_info.size = UART_RECEIVE_LENGTH - 5;
#else
            p1->down_info.size = UART_TO_LRM_RECEIVE_LENGTH;
#endif
            if(0 == p1->down_info.size)
            {
                p1->down_info.business_data = NULL;
            }
            else
            {
                p1->down_info.business_data = (uint8_t *)malloc(p1->down_info.size);
            }

            if(NULL == p1->down_info.business_data && 0 != p1->down_info.size)
            {
                free(p1);
#ifdef DEBUG_LOG_LEVEL_0
                DEBUG_PRINTF ("Error��Dynamic application for memory failure -- p1->down_info.business_data\r\n");
#endif
            }
            else
            {
#ifdef USE_NODE_STATUS
                for(cnt = 1; cnt <= p1->down_info.size; cnt++)
                {
                    *(p1->down_info.business_data + cnt - 1) = UART_RECEIVE_BUFFER[cnt];
                }
                p1->down_info.result_ind = UART_RECEIVE_BUFFER[cnt++];
                p1->down_info.snr = UART_RECEIVE_BUFFER[cnt++];
                p1->down_info.rssi = UART_RECEIVE_BUFFER[cnt++];
                p1->down_info.datarate = UART_RECEIVE_BUFFER[cnt] & 0x0f;
#else
                for(cnt = 0; cnt < p1->down_info.size; cnt++)
                {
                    *(p1->down_info.business_data + cnt) = UART_TO_LRM_RECEIVE_BUFFER[cnt];
                }
#endif
                p1->next = NULL;

                if(NULL != *list_head)
                {
                    p2->next = p1;
                }
                else
                {
                    *list_head = p1;
                }
            }
        }
        else
        {
#ifdef DEBUG_LOG_LEVEL_0
            DEBUG_PRINTF ("Error��Dynamic application for memory failure -- p1\r\n");
#endif
        }
#ifdef USE_NODE_STATUS
    }
    last_up_datarate = UART_RECEIVE_BUFFER[UART_RECEIVE_LENGTH - 1] & 0x0f;
#endif
}


/**
 * @brief   ����������ʽ��������
 * @details
 * @param   [IN]buffer: Ҫ���͵����ݰ�������
 * @param	[IN]size: Ҫ���͵����ݰ��ĳ���
 * @param	[OUT]list_head: ָ��������Ϣ������ͷ��ָ�룬ָ���������ֻ�а���ҵ�����ݻ��ڿ���ģ���STATUSָ���ǲ�����ҵ�����ݵ����һ����Ϣ
 * @return  ����ͨ�ŵ�״̬
 * @retval		00000001B -01- COMMUNICATION_SUCCESSS��ͨ�ųɹ�
 * @retval		00000010B -02- NO_JOINED��ģ��δ����
 * @retval		00000100B -04- COMMUNICATION_FAILURE��ȷ��֡δ�յ�ACK
 * @retval		00001000B -08- NODE_BUSY��ģ�鵱ǰ����æ״̬
 * @retval		00010000B -16- NODE_EXCEPTION��ģ�鴦���쳣״̬
 * @retval		00100000B -32- NODE_NO_RESPONSE��ģ�鴮������Ӧ
 * @retval		01000000B -64- PACKET_LENGTH_ERROR�����ݰ����ȴ���
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API����������
 *
 * @code
 * uint8_t test_data[100];
 * down_list_t *head = NULL;
 * execution_status_t send_result;
 *
 * //����51�ֽ�ȷ��֡���ݣ��ط�����Ϊ4��
 * send_result = node_block_send(test_data, 51, &head);
 * DEBUG_PRINTF("Send result: %02x \n", send_result);
 * @endcode
 */
static execution_status_t node_block_send(uint8_t *buffer, uint8_t size, down_list_t **list_head)
{
    static uint8_t abnormal_count = 0;

    /* ��������η�����������ǰģ��һֱæ��ģ�鴮������Ӧ����λģ�鲢��������*/
    if(abnormal_count > ABNORMAL_CONTINUOUS_COUNT_MAX)
    {
        abnormal_count = 0;
        nodeResetJoin(300);
    }

    free_down_info_list(list_head, all_list);

    // 1.��������ǰ���ж�ģ���Ƿ�����
    if(false == node_join_successfully)
    {
        return NODE_NOT_JOINED;
    }
    // 2.�жϴ�������ݳ����Ƿ���ȷ
    if(0 == size)
    {
        return USER_DATA_SIZE_WRONG;
    }

    // 3.���߻򱣳�WAKE��Ϊ��
    node_gpio_set(wake, wakeup);

    // 4.�е�͸��ģʽ
    node_gpio_set(mode, transparent);

    // 5.��������ǰ�ж�ģ��BUSY�����Ƿ�Ϊ�͵�ƽ����Ϊ����ȴ�TIMEOUT�󷵻�
    if( nodePinBusyStatusHolding( low, 1000 ) == false )
    {
        abnormal_count ++;
        return NODE_BUSY_BFE_RECV_UDATA;
    }

    // 6.ͨ��������ģ�鷢������
    transfer_node_data(buffer, size);

    // 7.�ж�ģ��BUSY�Ƿ����ͣ���1s��δ���ͣ���ģ�鴮���쳣
    if( nodePinBusyStatusHolding( high, 1000 ) == false )
    {
        abnormal_count++;
        return NODE_IDLE_ATR_RECV_UDATA;
    }

    abnormal_count = 0;

    // 8.�ȴ�BUSY����
    do
    {
        /* ����ͨ�����ʱʱ�䣺60s */
        if( nodePinBusyStatusHolding( low, 60*1000 ) == false )
        {
            return NODE_BUSY_ATR_COMM;
        }

        /* ���BUSY��������ʱ������100ms,˵���������� */
        nodePinBusyStatusHolding( high, 100 );

        /* �Ƿ����������ݻ�������ͨ����־���ȴ����ж� */
        timeout_start_flag = true;
        while(0 == UART_TO_LRM_RECEIVE_FLAG)
        {
            if(true == time_out_break_ms(300))
            {
                break;
            }
        }

        if(1 == UART_TO_LRM_RECEIVE_FLAG && UART_TO_LRM_RECEIVE_LENGTH > 0)
        {
            UART_TO_LRM_RECEIVE_FLAG = 0;

            /* �����������ݻ�ͨ����־��PC */
            UART_TO_PC_WRITE_DATA(UART_TO_LRM_RECEIVE_BUFFER,UART_TO_LRM_RECEIVE_LENGTH);

            /* ��ȡ�������� */
            down_data_process(list_head);
        }
        else
        {
            UART_TO_LRM_RECEIVE_LENGTH = 0;
        }

        /* �Ƿ�������ͨ��ʧ��32�� */
        if(31 == confirm_continue_failure_count)
        {
            if(low == node_gpio_read(busy) && UART_TO_LRM_RECEIVE_LENGTH < 10)
            {
                // �ж�BUSY��Ϊ�ͺ��ѯģ�鵱ǰ�ѷ��Ѿ���������ע��״̬
                if(1 == handle_cmd_return_data("AT+JOIN?", 1, 1))
                {
                    confirm_continue_failure_count = handle_cmd_return_data("AT+STATUS0?", 37, 2);
                }
                else
                {
                    // ȷ��ģ���ѷ�������ע�ᣬ�����֮ǰ������״̬���º�ȷ��֡����ʧ�ܼ���
                    node_join_successfully = false;
                    confirm_continue_failure_count = 0;
#ifdef DEBUG_LOG_LEVEL_1
                    DEBUG_PRINTF("Start Rejoin...\r\n");
#endif
                    node_join_successfully = node_block_join(300);

                    return NODE_COMM_NO_ACK;
                }
            }
        }
    } while(low == node_gpio_read(busy));


    // 10.ͨ��STAT�����жϵ�ǰͨ��״̬
    if(low == node_gpio_read(stat))
    {
        /* ���ð�Ϊȷ��֡��δ�յ�ACK����ȷ��֡����ʧ�ܼ�����1 */
        confirm_continue_failure_count += 1;
        return NODE_COMM_NO_ACK;
    }
    else
    {
        /* ͨ�ųɹ� */
        confirm_continue_failure_count = 0;
        return NODE_COMM_SUCC;
    }
}


/**
* ������һ���ְ����ȴ�ASӦ�𣨽���node_block_send_big_packet�������ã�
*@param		packet_end: ���һ���ְ��ĵ�ַ
*@param		size: ���һ���ְ��ĳ���
*@param		packet_mark: �����ݰ������
*@param		resend_num: ���һ��С���ݰ���δ�յ�Ӧ�÷�������AS���ض�Ӧ�������£����һ��С���ݰ����ط�����
*@return    AS�����зְ��Ľ��ս��
*/
static uint32_t wait_as_respone(uint8_t *packet_end, uint8_t size, uint8_t packet_mark, uint8_t resend_num, down_list_t **list_head)
{
    uint8_t resend_count = 0;
    uint32_t receive_success_bit = 0;
    bool receive_as_answer = false;
    bool first_point = true;
    down_list_t *_list_head = NULL;
    down_list_t *list_tmp = NULL;
    down_list_t *list_storage = NULL;

    while(resend_count++ <= resend_num)
    {
#ifdef DEBUG_LOG_LEVEL_1
        uint8_t tmp = 0;
        DEBUG_PRINTF("[SEND-LAST] ");
        for(tmp = 0; tmp < size; tmp++)
        {
            DEBUG_PRINTF("%02x ", packet_end[tmp]);
        }
        DEBUG_PRINTF("\r\n");
#endif

        _list_head = NULL;
        node_block_send(packet_end, size, &_list_head);

        list_tmp = _list_head;

        while(NULL != list_tmp)
        {
            if(list_tmp->down_info.size > 0)
            {
                if(0xBB == list_tmp->down_info.business_data[0] && packet_mark == list_tmp->down_info.business_data[1] &&
                        0xAA == list_tmp->down_info.business_data[list_tmp->down_info.size - 1] && 7 == list_tmp->down_info.size)
                {
                    receive_success_bit = list_tmp->down_info.business_data[2] | (list_tmp->down_info.business_data[3] << 8) | \
                                          (list_tmp->down_info.business_data[4] << 16) | (list_tmp->down_info.business_data[5] << 24);
                    // �ְ�Э�����ݲ�����ҵ�����ݣ��������Ǵ���е����һ�����У����ݱ�������״ֵ̬
                    list_tmp->down_info.size = 0;
                    receive_as_answer = true;
                }

#ifndef USE_NODE_STATUS
                else
                {
#endif
                    if(0 != list_tmp->down_info.size || NULL == list_tmp->next)
                    {
                        if(true == first_point)
                        {
                            first_point = false;
                            list_storage = list_tmp;
                            *list_head = list_storage;
                        }
                        else
                        {
                            list_storage->next = list_tmp;
                            list_storage = list_storage->next;
                        }
                    }

#ifndef USE_NODE_STATUS
                }
#endif
            }
#ifdef USE_NODE_STATUS
            else
            {
                if(true == receive_as_answer)
                {
                    list_storage->next = list_tmp;
                    list_storage = list_storage->next;
                }
                else if(resend_count > resend_num)
                {
                    if(true == first_point)
                    {
                        first_point = false;
                        list_storage = list_tmp;
                        *list_head = list_storage;
                    }
                    else
                    {
                        list_storage->next = list_tmp;
                        list_storage = list_storage->next;
                    }
                }
            }
#endif
            list_tmp = list_tmp->next;
        }

        if(resend_count > resend_num)
        {
            free_down_info_list(&_list_head, save_data_and_last);
        }
        else
        {
            if(true == receive_as_answer)
            {
                free_down_info_list(&_list_head, save_data_and_last);
                break;
            }
            else
            {
                free_down_info_list(&_list_head, save_data);
            }
        }
    }

    return receive_success_bit;
}


/**
 * @brief   �ͷ�������Ϣ��������Դ
 * @details �ͷ�������Ϣ��������Դ ��node_block_send()��node_block_send_big_packet()���ѵ��øú����ͷ���Ӧ����Դ
 * @param   [IN]list_head: ������ͷ
 * @param   [IN]free_level: �ͷŵȼ�
 *			- all_list����˳��ȫ���ͷ�
 *			- save_data������������data��Ϊ�յ���Դ
 *			- save_data_and_last������������data��Ϊ�յ���Դ�����һ����Դ
 * @return  ��
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API���ͷ�������Դ
 *
 * @code
 * //�ͷ�list_head��ָ���������Դ
 * free_down_info_list(&list_head, all_list);
 * @endcode
 */
static void free_down_info_list(down_list_t **list_head, free_level_t free_level)
{
    down_list_t *p1 = NULL;
    down_list_t *p2 = NULL;

    p1 = *list_head;

    while(p1)
    {
        p2 = p1->next;

        switch(free_level)
        {
        case all_list:
            if(NULL != p1->down_info.business_data)
            {
                free(p1->down_info.business_data);
            }
            free(p1);
            break;
        case save_data:
            if(p1->down_info.size == 0)
            {
                if(NULL != p1->down_info.business_data)
                {
                    free(p1->down_info.business_data);
                }
                free(p1);
            }
            break;
        case save_data_and_last:
            if(p1->down_info.size == 0 && NULL != p2)
            {
                if(NULL != p1->down_info.business_data)
                {
                    free(p1->down_info.business_data);
                }
                free(p1);
            }
            break;
        }

        p1 = p2;
    }

    *list_head = NULL;
}


/**
* ģ��Ӳ����λ���������û�ֱ��ʹ�øú����������û�ʹ��node_hard_reset_and_configure()��Ӳ��λģ��
*/
static void node_hard_reset(void)
{
#ifdef DEBUG_LOG_LEVEL_1
    DEBUG_PRINTF("Node hard reset\r\n");
#endif
    // ģ�鸴λǰ��WAKE�������ߣ���Ҫ��
    node_gpio_set(wake, wakeup);
    SET_RESET_LOW;
    system_delay_ms(15);
    SET_RESET_HIGH;
    system_delay_ms(200);
}


/**
*@brief		��ȡģ���ͨ�Ž������ӡ
*@param		send_result��ģ���ͨ�Ž��
*@return	NULL
 */
static void commResultPrint(execution_status_t send_result)
{
    debug_printf("send_result = %d, ",send_result);
    switch(send_result)
    {
    case NODE_COMM_SUCC:
        debug_printf("node comm success.\r\n");
        break;
    case NODE_NOT_JOINED:
        debug_printf("node not joined.\r\n");
        break;
    case NODE_COMM_NO_ACK:
        debug_printf("node comm no ack.\r\n");
        break;
    case NODE_BUSY_BFE_RECV_UDATA:
        debug_printf("node keep busy before recv user's data.\r\n");
        break;
    case NODE_BUSY_ATR_COMM:
        debug_printf("node keep busy after comm.\r\n");
        break;
    case NODE_IDLE_ATR_RECV_UDATA:
        debug_printf("node keep idle atfer recv usr's data.\r\n");
        break;
    case USER_DATA_SIZE_WRONG:
        debug_printf("usr's data size is wrong.\r\n");
        break;
    default:
        break;
    }
}


/*------------------------------------------------------------------------------
|>                        ����Ϊ���û��ṩ�ĵ��ú���                          <|
------------------------------------------------------------------------------*/
/** @defgroup Module_with_LoRaWAN_Driver_Exported_Functions Module_Driver_API_Interface
  * @brief LoRaWANģ���������û�API�ӿ�,��Ҫ��������ģ����и�λ��AT�������á��������������ݵ�
  * @{
  */


/**
 * @brief   ģ����������
 * @details ����ģ��wake��mode���ŵ��û��ӿ�
 * @param   ��
 * @return  ��
 */
void nodeGpioConfig(node_gpio_t type, node_status_t value)
{
    node_gpio_set(type,value);
}


/**
 * @brief   Ӳ����λ
 * @details ͨ��Reset���Ÿ�λģ��.
 * @param   ��.
 * @return  ��
 */
void Node_Hard_Reset(void)
{
    node_hard_reset();
}


/**
*@brief		����ָ���ӡ���ý��
*@param		str: ��Ҫ���õ�ָ��
*@return	true: ���óɹ�; false: ����ʧ��
 */
bool nodeCmdConfig(char *str)
{
    return transfer_configure_command(str);
}


/**
*@brief		��ѯָ���ӡ��ѯ���
*@param		str: ��Ҫ��ѯ��ָ��; content: ���ص�ָ������
*@return	true: ��ѯ�ɹ�; false: ��ѯʧ��
 */
bool nodeCmdInqiure(char *str,uint8_t *content)
{
    return transfer_inquire_command(str,content);
}


/**
*@brief		ģ���������ò���ӡ�������
*@param		time_second: ������ʱʱ��
*@return	true:�����ɹ���false:����ʧ��
 */
bool nodeJoinNet(uint16_t time_second)
{
    return node_block_join(time_second);
}


/**
 * @brief   ��λ�ڵ㲢����ɨ��
 * @details ��λ�ڵ㲢����������ʽ��������ɨ��
 * @param   [IN]time_second: ������ʱʱ�䣬��λ����
 * @return  ���������Ľ��
 * @retval		true:  �����ɹ�
 * @retval		false: ����ʧ��
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API�����и�λ�ڵ㲢����ɨ��
 * @code
 * // ��λ�ڵ㲢����ɨ�裬��ʱʱ������Ϊ300�루��ʱʱ���ƽ�⹦���������ɹ����趨��
 * node_reset_block_join(300);
 * if(true == node_join_successfully)
 * {
 *   // �ѳɹ�����
 * }
 * else
 * {
 *	 // ����ʧ��
 * }
 * @endcode
 */
void nodeResetJoin(uint16_t time_second)
{
    node_join_successfully = false;

    // Ӳ����λģ��, ������һЩ���粻�����ָ��
    Node_Hard_Reset();

    if(false == node_join_successfully)
    {
        node_join_successfully = node_block_join(time_second);
    }

    node_gpio_set(wake, sleep);
}



/**
*@brief		ģ������ͨ�Ų���ӡͨ�Ž��
*@param		buffer: �û�����; size: �û����ݴ�С; list_head: ������������
*@return	send_result��ģ���ͨ�Ž��
 */
execution_status_t nodeDataCommunicate(uint8_t *buffer, uint8_t size, down_list_t **list_head)
{
    uint8_t buf_tmp[255] = {0};
    execution_status_t send_result;

    memcpy(buf_tmp,buffer,size);
#ifdef DEBUG_LOG_LEVEL_1
    /* ��ӡ�û����������Ϣ */
    DEBUG_PRINTF("--send message: %s--size: %d\r\n", buf_tmp, size);
#endif

    send_result = node_block_send(buffer, size, list_head);

#ifdef DEBUG_LOG_LEVEL_1
    commResultPrint(send_result);
#endif
    return send_result;
}

#if ( M_ADVANCED_APP > 0 )
/**
 * @brief   ����ģ��������������������
 * @details ģ�鴦����������������ʱ������ģ��������������ͨ��node_join_successfully��ֵ�ж��Ƿ�ɹ�����
 * @param   [IN]time_second: ������ʱʱ�䣬��λ����
 * @return  ���������Ľ��
 * @retval		true:  �����ɹ�
 * @retval		false: ����ʧ��
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API����������������������
 * @code
 *
 * if(hot_start_rejoin(300) == true && node_join_successfully == true)
 * {
 *   // ����������
 * }
 * else
 * {
 *	 // ��������ʧ��
 * }
 * @endcode
 */
bool hot_start_rejoin(uint16_t time_second)
{
    bool cmd_result = transfer_configure_command("AT+JOIN=1");

    if(true == cmd_result)
    {
        node_join_successfully = node_block_join(time_second);
    }

    return cmd_result;
}


/**
 * @brief   ��ģ�����ߵķ��ͺ���
 * @details ����������ʽ�������ݣ�����ǰ����ģ�飬������ɺ�����ģ��
 * @param	[IN]frame_type: ʮ�����ƣ���ʾ���η��͵�֡���ͺͷ��ʹ���\n
						����λ��Ҫ���͵�֡Ϊȷ��֡(0001)���Ƿ�ȷ��(0000),\n
						����λ����ȷ��֡�����ʾδ�յ�ACK������¹����͵Ĵ�������Ϊ��ȷ�ϣ����ʾ�ܹ��跢�͵Ĵ���
 * @param   [IN]buffer: Ҫ���͵����ݰ�������
 * @param	[IN]size: Ҫ���͵����ݰ��ĳ���
 * @param	[OUT]list_head: ָ��������Ϣ������ͷ��ָ�룬ָ���������ֻ�а���ҵ�����ݻ��ڿ���ģ���STATUSָ���ǲ�����ҵ�����ݵ����һ����?
 * @return  ����ͨ�ŵ�״̬
 * @retval		00000001B -01- COMMUNICATION_SUCCESSS��ͨ�ųɹ�
 * @retval		00000010B -02- NO_JOINED��ģ��δ����
 * @retval		00000100B -04- COMMUNICATION_FAILURE��ȷ��֡δ�յ�ACK
 * @retval		00001000B -08- NODE_BUSY��ģ�鵱ǰ����æ״̬
 * @retval		00010000B -16- NODE_EXCEPTION��ģ�鴦���쳣״̬
 * @retval		00100000B -32- NODE_NO_RESPONSE��ģ�鴮������Ӧ
 * @retval		01000000B -64- PACKET_LENGTH_ERROR�����ݰ����ȴ���
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API����������
 *
 * @code
 * uint8_t test_data[100];
 * down_list_t *head = NULL;
 * execution_status_t send_result;
 *
 * //����51�ֽ�ȷ��֡���ݣ��ط�����Ϊ3��
 * send_result = node_block_send_lowpower(CONFIRM_TYPE | 0x03, test_data, 51, &head);
 * DEBUG_PRINTF("Send result: %02x \n", send_result);
 * @endcode
 */
execution_status_t node_block_send_lowpower(uint8_t frame_type, uint8_t *buffer, uint8_t size, down_list_t **list_head)
{
    execution_status_t status_result;

    // 1.����node_block_send�����������ݲ��õ�����
    status_result = node_block_send(buffer, size, list_head);
    // 2.����������ɺ�����ģ��
    node_gpio_set(wake, sleep);

    return status_result;
}


/**
 * @brief   �����ݰ���������
 * @details �������ݰ����ݲ�ͬ�����ʲ�ֳɶ��С���ݰ��ֱ���(���֧�ֲ�ֳ�32��С��)��ʹ�øú���ʱ��AS��Ӧ�÷���������ʵ�ֱ���������ķְ�Э�飬�ú�����ʵ���ն˵ķְ�Э��
 * �ְ�Э�����£�\n
  1. ���а��ṹ���ն˵�Ӧ�÷�������
 * 	  - ��һ���ֽ�Ϊ��ͷ���̶�Ϊ0xAA
 *	  - �ڶ����ֽ�Ϊ�����ݰ������
 *	  - �������ֽڵ�7bitΪӦ������λ��1��ʾ����Ӧ��0��ʾAS����Ӧ�𣩣���5~6bit��������0~4bitΪ�ְ��İ����
 *	  - ���һ���ֽ�Ϊ��β���̶�Ϊ0xBB
 *	  - �����ֽ�Ϊ�û�����
  2. ���а��ṹ��Ӧ�÷��������նˣ�
 *	   - ��һ���ֽ�Ϊ��ͷ���̶�Ϊ0xBB
 *	   - �ڶ����ֽڱ�ʾ��ǰ�����ݰ������
 *	   - �����������ֽڱ�ʾӦ�÷������Ľ��ոô����ݰ����ְ��Ľ������λ��ʾ��С��ģʽ���ĸ��ֽڹ�32λ, ����յ��ķְ����Ϊ��0��1��2��5��6��7��8��10��������������ֽ�����Ϊ��0xE7 0x05 0x00 0x00
 *	   - ���һ���ֽ�Ϊ��β���̶�Ϊ0xAA
  3. �ն˷ְ��߼�
 *	   - 3.1 �ر�ADR���ն˸��ݵ�ǰģ������ʽ������ݰ���ֳ����ɸ��ְ������Ӹ��ĸ��ְ�Э���ֽڣ��Է�ȷ��֡���ͳ�ȥ
 *	   - 3.2 �������һ���ְ�ʱ���ð�Ӧ������λ��1�����ȴ�ASӦ����δ�յ�AS��Ӧ�����ط����һ�������ط����趨�Ĵ������δ�յ�AS����ȷӦ���򷵻ش����ݰ�����ʧ��
 *	   - 3.3 �ն˸���ASӦ������ݽ�����ASδ�յ���Щ�ְ���Ȼ�����·�����Щδ��AS���յķְ���������Щ�ְ��е����һ�����Ϊ����������ִ��3.2������
 *	   - 3.4 ��ASӦ�������Ϊ�ɹ����������еķְ��󣬽��������ݰ��������̣����ؽ��
  4. Ӧ�÷�����(AS)�����Ӧ���߼�
 *	   - 4.1 ASÿ����һ�����ݺ���ݰ�ͷ�Ͱ�β�жϸð��Ƿ�Ϊ�����ݰ��ķְ�
 *	   - 4.2 ���ð�Ϊ�����ݰ��ķְ������жϸð������ݰ����������һ�ְ��Ƿ���ͬ�������ͬ���򽫡����ձ��±�������uint32������
 *	   - 4.3 ���ݸ÷ְ���Ž������ձ��±�������Ӧ��λ��1���жϸ÷ְ��Ƿ���ҪӦ��������Ӧ���򽫸ð��ݴ�����
 *	   - 4.4 ��4.3���жϳ��÷ְ���Ӧ���򽫡����ձ��±����� �����а��ṹҪ����������͸��նˣ������ݵ�ǰ�����ݰ�����µĸ��ְ�����ж��Ƿ��ѽ������зְ�
 *		 	�����ְ���ţ�ͬһ�����ݰ�����У�����Ӧ������λΪ1�ķְ��зְ���������ǰ���
 *	   - 4.5 ��4.4���ж�Ϊ��ȫ�����գ��򰴵�ǰ�����ݰ�����µķְ���Ŷ��û����ݽ�����������ܻ����ظ��ķְ������ȥ���������һ�������ݰ��Ľ�������
 * ע�����ʹ����ݰ�ʱĬ��ʹ�÷�ȷ��֡���Լ�С�Կտ���Դ��ռ��
 *
 * @param	[IN]buffer: Ҫ���͵����ݰ�������
 * @param	[IN]size: Ҫ���͵����ݰ��ĳ���
 * @param	[IN]resend_num: ���һ��С���ݰ���δ�յ�Ӧ�÷�������AS���ض�Ӧ�������£����һ��С���ݰ����ط�����
 * @param	[OUT]list_head: ָ��������Ϣ������ͷ��ָ�룬ָ���������ֻ�а���ҵ�����ݻ򲻰���ҵ�����ݵ����һ����Ϣ
 * @return  ���δ����ݰ������Ƿ�ɹ�
 * @retval		true: �����ݰ�������ȫ�ɹ�
 * @retval		false: �����ݰ�����ʧ��
 *
 * @par ʾ��:
 * ������ʾ��ε��ø�API�����ʹ����ݰ�
 *
 * @code
 * uint8_t test_data[810];
 * down_list_t *head = NULL;
 *
 * //����800�ֽڵĴ����ݰ������һ���ְ���δ�յ�Э��ظ���������ط�8��
 * if(node_block_send_big_packet(test_data, 800, 8, &head) == true)
 * {
 *   // �������ݳɹ�
 * }
 * // ��������ʧ��
 * @endcode
 */
bool node_block_send_big_packet(uint8_t *buffer, uint16_t size, uint8_t resend_num, down_list_t **list_head)
{
    uint8_t datarate = 0;
    uint8_t max_data_len[6] = {47, 47, 47, 111, 218, 218};  /* X - 4 example: SF7 222 - 4 = 218 */
    uint8_t sub_full_size = 0;
    uint8_t sub_last_size = 0;
    uint8_t i = 0, j = 0;
    uint8_t sended_count = 0;
    uint16_t packet_start_bit[32] = {0};
    uint8_t packet_length[32] = {0};
    uint8_t spilt_packet_data[222] = {0};
    uint8_t sub_fcnt = 0;
    uint8_t sub_fcnt_end = 0;
    uint32_t communication_result_mask = 0;
    uint32_t full_success_mask = 0;
    static uint8_t packet_mark = 0;
    bool first_point = true;

    down_list_t *single_list = NULL;
    down_list_t *single_list_head = NULL;
    down_list_t	*tmp_storage = NULL;
    down_list_t	*last_packet_list = NULL;
    down_list_t	*last_packet_list_head = NULL;

    free_down_info_list(list_head, all_list);

    node_gpio_set(wake, wakeup);

    transfer_configure_command("AT+ADR=0");

    if(-1 == last_up_datarate)
    {
        datarate = handle_cmd_return_data("AT+DATARATE?", 1, 1);
    }
    else
    {
        datarate = last_up_datarate;
    }
    // ���ݲ�ص�����ͨ�������֪��ǰ�ܷ����û����ݵ���󳤶�
    sub_full_size = max_data_len[datarate]; /*��ʾģ�鷵�صĵ�ǰ����*/
    sub_last_size = size % sub_full_size;
    // ÿ����һ�������ݰ��������ż�һ
    packet_mark++;

    // ����ÿ���ְ��ڴ������ʼλ�úͷְ��ĳ���
    for(sub_fcnt = 0; sub_fcnt < size / sub_full_size + 1; sub_fcnt++, i = 0)
    {
        packet_start_bit[sub_fcnt] = sub_fcnt * sub_full_size;

        if(sub_fcnt == size / sub_full_size)
        {
            packet_length[sub_fcnt] = sub_last_size + 4;
        }
        else
        {
            packet_length[sub_fcnt] = sub_full_size + 4;
        }
    }

    sub_fcnt--;

    // �������зְ������������ɹ����պ������
    for(i = 0; i <= sub_fcnt; i++)
    {
        full_success_mask |= (0x01 << sub_fcnt) >> i;
    }

    while(full_success_mask != (communication_result_mask & full_success_mask))
    {
        // ���Զ�ʧ���Ĳ����������ڷְ��ĸ������������Ӧ�÷������������ݴ��������쳣
        if(sended_count > sub_fcnt)
        {
            node_gpio_set(wake, sleep);
            return false;
        }
        // �������һ���ķְ����
        for(j = 0; j <= sub_fcnt; j++)
        {
            if(0 == ((communication_result_mask >> j) & 0x01))
            {
                sub_fcnt_end = j;
            }
        }
        // ���շְ�Э����û����ݲ�ֲ��������
        for(j = 0; j <= sub_fcnt; j++)
        {
            if(0 == ((communication_result_mask >> j) & 0x01))
            {
                // �ְ��İ�ͷ
                spilt_packet_data[0] = 0xAA;
                // �����ݰ������
                spilt_packet_data[1] = packet_mark;
                // ���ֽڵ�0~4bitΪ�ְ��İ���ţ���5~6bit��������7bitΪӦ������λ
                spilt_packet_data[2] = j & 0x1f;
                // �м�Ϊ�û�����
                for(i = 0; i < packet_length[j] - 4; i++)
                {
                    spilt_packet_data[i + 3] = *(buffer + packet_start_bit[j] + i);
                }
                // �ְ��İ�β
                spilt_packet_data[i + 3] = 0xBB;

                if(j == sub_fcnt_end)
                {
                    // ����ǰΪ���зְ��е����һ������ڶ����ֽڵ����λ�ã�֪ͨӦ�÷��������ն�(AS�ɹ��յ�����Щ�ְ�)
                    spilt_packet_data[2] |= 0x80;
                }
                else
                {
#ifdef DEBUG_LOG_LEVEL_1
                    uint8_t tmp = 0;
                    DEBUG_PRINTF("[SEND] ");
                    for(tmp = 0; tmp < packet_length[j]; tmp++)
                    {
                        DEBUG_PRINTF("%02x ", spilt_packet_data[tmp]);
                    }
                    DEBUG_PRINTF("\r\n");
#endif

                    single_list = NULL;
                    node_block_send(spilt_packet_data, packet_length[j], &single_list);

                    // ���´����Ϊʹ�������Խ��յ���ģ�鴮����������Ӧ�Ĵ�����ֻ������ҵ�����ݵĲ���
                    single_list_head = single_list;

                    while(NULL != single_list)
                    {
                        if(single_list->down_info.size > 0)
                        {
                            if(true == first_point)
                            {
                                first_point = false;
                                tmp_storage = single_list;
                                *list_head = tmp_storage;
                            }
                            else
                            {
                                tmp_storage->next = single_list;
                                tmp_storage = tmp_storage->next;
                            }
                        }
                        single_list = single_list->next;
                    }
                    free_down_info_list(&single_list_head, save_data);
                }
            }
        }

        // �������һ���ְ������ȴ�ASӦ��
        last_packet_list = NULL;
        communication_result_mask = wait_as_respone(spilt_packet_data,
                                    packet_length[sub_fcnt_end],
                                    packet_mark,
                                    resend_num,
                                    &last_packet_list);

        // ���´����Ϊʹ���������Խ��յ���ģ�鴮����������Ӧ�Ĵ�����ֻ������ҵ�����ݵĲ��ֻ����зְ������һ����������Ϣ
        last_packet_list_head = last_packet_list;

        if(full_success_mask == (communication_result_mask & full_success_mask) ||
                0 == communication_result_mask || sended_count++ == sub_fcnt)
        {
            while(NULL != last_packet_list)
            {
                if(true == first_point)
                {
                    first_point = false;
                    tmp_storage = last_packet_list;
                    *list_head = tmp_storage;
                }
                else
                {
                    tmp_storage->next = last_packet_list;
                    tmp_storage = tmp_storage->next;
                }

                last_packet_list = last_packet_list->next;
            }

            free_down_info_list(&last_packet_list_head, save_data_and_last);
        }
        else
        {
            while(NULL != last_packet_list)
            {
                if(last_packet_list->down_info.size > 0)
                {
                    if(true == first_point)
                    {
                        first_point = false;
                        tmp_storage = last_packet_list;
                        *list_head = tmp_storage;
                    }
                    else
                    {
                        tmp_storage->next = last_packet_list;
                        tmp_storage = tmp_storage->next;
                    }
                }

                last_packet_list = last_packet_list->next;
            }

            free_down_info_list(&last_packet_list_head, save_data);
        }

        if(0 == communication_result_mask)
        {
#ifdef DEBUG_LOG_LEVEL_1
            DEBUG_PRINTF("Did not receive a reply from AS\r\n");
#endif
            node_gpio_set(wake, sleep);
            // ��ASδӦ���ն˻����Ӧ���򱾴δ����ݰ�ͨ��ʧ��
            return false;
        }
#ifdef DEBUG_LOG_LEVEL_1
        DEBUG_PRINTF("full_success_mask: 0x%08x, communication_result_mask: 0x%08x\r\n", full_success_mask, communication_result_mask);
#endif
    }

    transfer_configure_command("AT+ADR=1");
    node_gpio_set(wake, sleep);

    return true;
}

#endif //M_ADVANCED_APP 0
/**
  * @} Module_Driver_API_Interface
  */


/*! @} defgroup ICA-Driver */