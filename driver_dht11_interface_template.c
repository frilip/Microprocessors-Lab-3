/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_dht11_interface_template.c
 * @brief     driver dht11 interface template source file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-03-12
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/03/12  <td>2.0      <td>Shifeng Li  <td>format the code
 * <tr><td>2020/11/19  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_dht11_interface.h"

/**
 * @brief  interface bus init
 * @return status code
 *         - 0 success
 *         - 1 bus init failed
 * @note   none
 */
uint8_t dht11_interface_init(void)
{
		
		
    return 0;
}

/**
 * @brief  interface bus deinit
 * @return status code
 *         - 0 success
 *         - 1 bus deinit failed
 * @note   none
 */
uint8_t dht11_interface_deinit(void)
{
    return 0;
}

/**
 * @brief      interface bus read
 * @param[out] *value pointer to a value buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t dht11_interface_read(uint8_t *value)
{
    return 0;
}

/**
 * @brief     interface bus write
 * @param[in] value written value
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t dht11_interface_write(uint8_t value)
{
    return 0;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void dht11_interface_delay_ms(uint32_t ms)
{
    
}

/**
 * @brief     interface delay us
 * @param[in] us time
 * @note      none
 */
void dht11_interface_delay_us(uint32_t us)
{
    
}

/**
 * @brief interface enable the interrupt
 * @note  none
 */
void dht11_interface_enable_irq(void)
{
    
}

/**
 * @brief interface disable the interrupt
 * @note  none
 */
void dht11_interface_disable_irq(void)
{
    
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void dht11_interface_debug_print(const char *const fmt, ...)
{
    
}
