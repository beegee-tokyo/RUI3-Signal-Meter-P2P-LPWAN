/**
 * @file RUI3-Signal-Meter-P2P-LPWAN.ino
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief Simple signal meter for LoRa P2P and LoRaWAN
 * @version 0.1
 * @date 2023-11-23
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <Arduino.h>
#include "RAK1921_oled.h"

// Set firmware version
#define SW_VERSION_0 1
#define SW_VERSION_1 0
#define SW_VERSION_2 1

/** Flag if confirmed packets or LinkCheck should be used */
bool use_link_check = true;

/** Last RX SNR level*/
int8_t last_snr = 0;
/** Last RX RSSI level*/
int16_t last_rssi = 0;
/** Link check result */
uint8_t link_check_state;
/** Demodulation margin */
uint8_t link_check_demod_margin;
/** Number of gateways */
uint8_t link_check_gateways;
/** Sent packet counter */
int32_t packet_num = 0;
/** Lost packet counter (only LPW mode)*/
int32_t packet_lost = 0;
/** Last RX data rate */
uint8_t last_dr = 0;
/** TX fail reason (only LPW mode)*/
int32_t tx_fail_status;

/** Flag if OLED was found */
bool has_oled = false;
/** Buffer for OLED output */
char line_str[256];
/** LoRa mode */
bool lorawan_mode = true;
/** Flag for display handler */
uint8_t display_reason;

/**
 * @brief Send a LoRaWAN packet
 *
 * @param data unused
 */
void send_packet(void *data)
{
	Serial.println("Send packet");
	uint8_t payload[4] = {0x01, 0x02, 0x03, 0x04};
	if (use_link_check)
	{
		// Linkcheck is enabled, send an unconfirmed packet
		api.lorawan.send(4, payload, 2, false);
	}
	else
	{
		// Linkcheck is disabled, send a confirmed packet
		api.lorawan.send(4, payload, 2, true, 8);
	}
}

/**
 * @brief Display handler
 *
 * @param reason 1 = RX packet display
 *               2 = TX failed display (only LPW mode)
 *               3 = Join failed (only LPW mode)
 *               4 = Linkcheck result display (only LPW LinkCheck mode)
 */
void handle_display(void *reason)
{
	/** Update header and battery value every 10th packet */
	if (((packet_num % 10) == 0) && has_oled)
	{
		sprintf(line_str, "RAK Signal Meter - B %.2fV", api.system.bat.get());
		rak1921_write_header(line_str);
	}

	// Get the wakeup reason
	uint8_t *disp_reason = (uint8_t *)reason;

	// Check if we have a reason
	if (disp_reason == NULL)
	{
		Serial.println("Bug in code!");
	}
	else if (disp_reason[0] == 1)
	{
		Serial.printf("RX_EVENT %d\n", disp_reason[0]);
		// RX event display
		if (lorawan_mode)
		{
			if (has_oled)
			{
				sprintf(line_str, "LPW Packet %d", packet_num);
				rak1921_add_line(line_str);
				sprintf(line_str, "DR %d", last_dr);
				rak1921_add_line(line_str);
				sprintf(line_str, "RSSI %d", last_rssi);
				rak1921_add_line(line_str);
				sprintf(line_str, "SNR %d", last_snr);
				rak1921_add_line(line_str);
				sprintf(line_str, "----------");
				rak1921_add_line(line_str);
			}
			Serial.printf("Packet # %d RSSI %d SNR %d\n", packet_num, last_rssi, last_snr);
			Serial.printf("DR %d\n", last_dr);
		}
		else
		{
			if (has_oled)
			{
				sprintf(line_str, "P2P Packet %d", packet_num);
				rak1921_add_line(line_str);
				sprintf(line_str, "F %.1f SF %d BW %d",
						(float)api.lora.pfreq.get() / 1000000.0,
						api.lora.psf.get(),
						(api.lora.pbw.get() + 1) * 125);
				rak1921_add_line(line_str);
				sprintf(line_str, "RSSI %d", last_rssi);
				rak1921_add_line(line_str);
				sprintf(line_str, "SNR %d", last_snr);
				rak1921_add_line(line_str);
				sprintf(line_str, "----------");
				rak1921_add_line(line_str);
			}
			Serial.printf("Packet # %d RSSI %d SNR %d\n", packet_num, last_rssi, last_snr);
			Serial.printf("F %.1f SF %d BW %d\n",
						  (float)api.lora.pfreq.get() / 1000000.0,
						  api.lora.psf.get(),
						  (api.lora.pbw.get() + 1) * 125);
		}
	}
	else if (disp_reason[0] == 2)
	{
		Serial.printf("TX_ERROR %d\n", disp_reason[0]);

		digitalWrite(LED_BLUE, HIGH);
		if (has_oled)
		{
			sprintf(line_str, "Packet %d", packet_num);
			rak1921_add_line(line_str);
			sprintf(line_str, "TX failed with status %d", tx_fail_status);
			rak1921_add_line(line_str);
		}
		Serial.printf("Packet %d\n", packet_num);
		Serial.printf("TX failed with status %d\n", tx_fail_status);

		switch (tx_fail_status)
		{
		case RAK_LORAMAC_STATUS_ERROR:
			sprintf(line_str, "Service error");
			break;
		case RAK_LORAMAC_STATUS_TX_TIMEOUT:
			sprintf(line_str, "TX timeout");
			break;
		case RAK_LORAMAC_STATUS_RX1_TIMEOUT:
			sprintf(line_str, "RX1 timeout");
			break;
		case RAK_LORAMAC_STATUS_RX2_TIMEOUT:
			sprintf(line_str, "RX2 timeout");
			break;
		case RAK_LORAMAC_STATUS_RX1_ERROR:
			sprintf(line_str, "RX1 error");
			break;
		case RAK_LORAMAC_STATUS_RX2_ERROR:
			sprintf(line_str, "RX2 error");
			break;
		case RAK_LORAMAC_STATUS_JOIN_FAIL:
			sprintf(line_str, "Join failed");
			break;
		case RAK_LORAMAC_STATUS_DOWNLINK_REPEATED:
			sprintf(line_str, "Dowlink frame error");
			break;
		case RAK_LORAMAC_STATUS_TX_DR_PAYLOAD_SIZE_ERROR:
			sprintf(line_str, "Payload size error");
			break;
		case RAK_LORAMAC_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS:
			sprintf(line_str, "Fcnt loss error");
			break;
		case RAK_LORAMAC_STATUS_ADDRESS_FAIL:
			sprintf(line_str, "Adress error");
			break;
		case RAK_LORAMAC_STATUS_MIC_FAIL:
			sprintf(line_str, "MIC error");
			break;
		case RAK_LORAMAC_STATUS_MULTICAST_FAIL:
			sprintf(line_str, "Multicast error");
			break;
		case RAK_LORAMAC_STATUS_BEACON_LOCKED:
			sprintf(line_str, "Beacon locked");
			break;
		case RAK_LORAMAC_STATUS_BEACON_LOST:
			sprintf(line_str, "Beacon lost");
			break;
		case RAK_LORAMAC_STATUS_BEACON_NOT_FOUND:
			sprintf(line_str, "Beacon not found");
			break;
		default:
			sprintf(line_str, "Unknown error");
			break;
		}
		Serial.printf("%s\n", line_str);
		Serial.printf("Lost %d packets\n", packet_lost);
		if (has_oled)
		{
			rak1921_add_line(line_str);
			sprintf(line_str, "Lost %d packets", packet_lost);
			rak1921_add_line(line_str);
			sprintf(line_str, "----------");
			rak1921_add_line(line_str);
		}
	}
	else if (disp_reason[0] == 3)
	{
		Serial.printf("JOIN_ERROR %d\n", disp_reason[0]);
		if (has_oled)
		{
			rak1921_add_line((char *)"!!!!!!!!!!!!!!!!!!!");
			rak1921_add_line((char *)"!!!!!!!!!!!!!!!!!!!");
			sprintf(line_str, "Join failed");
			rak1921_add_line(line_str);
			rak1921_add_line((char *)"!!!!!!!!!!!!!!!!!!!");
			rak1921_add_line((char *)"!!!!!!!!!!!!!!!!!!!");
		}
	}
	else if (disp_reason[0] == 4)
	{
		Serial.printf("LINK_CHECK %d\n", disp_reason[0]);
		// LinkCheck result event display
		if (has_oled)
		{
			/**

				last_rssi = data->Rssi;
				link_check_state = data->State;
				link_check_demod_margin = data->DemodMargin;
				link_check_gateways = data->NbGateways;
			 */
			rak1921_add_line(line_str);
			sprintf(line_str, "LinkCheck %s", link_check_state == 0 ? "OK" : "NOK");
			rak1921_add_line(line_str);
			if (link_check_state == 0)
			{
				sprintf(line_str, "LPW Packet %d GW # %d", packet_num, link_check_gateways);
				rak1921_add_line(line_str);
				sprintf(line_str, "DR %d", api.lorawan.dr.get());
				rak1921_add_line(line_str);
				sprintf(line_str, "RSSI %d SNR %d", last_rssi, last_snr);
				rak1921_add_line(line_str);
				sprintf(line_str, "Demod Margin %d", link_check_demod_margin);
				rak1921_add_line(line_str);
			}
			else
			{
				sprintf(line_str, "Lost %d packets", packet_lost);
				rak1921_add_line(line_str);
				sprintf(line_str, " ");
				rak1921_add_line(line_str);
				sprintf(line_str, " ");
				rak1921_add_line(line_str);
				sprintf(line_str, "----------");
				rak1921_add_line(line_str);
			}
		}
		Serial.printf("LinkCheck %s\n", link_check_state == 0 ? "OK" : "NOK");
		Serial.printf("Packet # %d RSSI %d SNR %d\n", packet_num, last_rssi, last_snr);
		Serial.printf("GW # %d Demod Margin %d\n", link_check_gateways, link_check_demod_margin);
	}

	digitalWrite(LED_GREEN, LOW);
}

/**
 * @brief Join network callback
 * 
 * @param status status of join request
 */
void join_cb_lpw(int32_t status)
{
	if (status != 0)
	{
		display_reason = 3;
		api.system.timer.start(RAK_TIMER_1, 250, &display_reason);
	}
}

/**
 * @brief Receive callback for LoRa P2P mode
 *
 * @param data structure with RX packet information
 */
void recv_cb_p2p(rui_lora_p2p_recv_t data)
{
	digitalWrite(LED_GREEN, HIGH);
	last_rssi = data.Rssi;
	last_snr = data.Snr;
	packet_num++;

	display_reason = 1;
	api.system.timer.start(RAK_TIMER_1, 250, &display_reason);
}

/**
 * @brief Receive callback for LoRaWAN mode
 *
 * @param data structure with RX packet information
 */
void recv_cb_lpw(SERVICE_LORA_RECEIVE_T *data)
{
	digitalWrite(LED_GREEN, HIGH);
	last_rssi = data->Rssi;
	last_snr = data->Snr;
	last_dr = data->RxDatarate;

	packet_num++;

	if (!use_link_check)
	{
		display_reason = 1;
		api.system.timer.start(RAK_TIMER_1, 250, &display_reason);
	}
}

/**
 * @brief Send finished callback for LoRaWAN mode
 *
 * @param status
 */
void send_cb_lpw(int32_t status)
{
	if (status != RAK_LORAMAC_STATUS_OK)
	{
		packet_lost++;
		tx_fail_status = status;

		if (!use_link_check)
		{
			display_reason = 2;
			api.system.timer.start(RAK_TIMER_1, 250, &display_reason);
		}
	}
}

/**
 * @brief Linkcheck callback
 * 
 * @param data structure with the result of the Linkcheck
 */
void linkcheck_cb_lpw(SERVICE_LORA_LINKCHECK_T *data)
{
	last_snr = data->Snr;
	last_rssi = data->Rssi;
	link_check_state = data->State;
	link_check_demod_margin = data->DemodMargin;
	link_check_gateways = data->NbGateways;

	display_reason = 4;
	api.system.timer.start(RAK_TIMER_1, 250, &display_reason);
}

/**
 * @brief Setup routine
 *
 */
void setup(void)
{
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	Serial.begin(115200);
	sprintf(line_str, "RUI3_Tester_V%d.%d.%d", SW_VERSION_0, SW_VERSION_1, SW_VERSION_2);
	api.system.firmwareVersion.set(line_str);

	digitalWrite(LED_GREEN, HIGH);
#ifndef RAK3172
	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial.available())
	{
		if ((millis() - serial_timeout) < 5000)
		{
			delay(100);
			digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
		}
		else
		{
			break;
		}
	}
#else
	digitalWrite(LED_GREEN, HIGH);
	delay(5000);
#endif

	digitalWrite(LED_GREEN, LOW);
	digitalWrite(LED_BLUE, LOW);

	// Check if we are in LoRa P2P or LoRaWAN mode
	if (api.lora.nwm.get() == 1)
	{
		lorawan_mode = true;
	}
	else
	{
		lorawan_mode = false;
	}

	// Check if OLED is available
	Wire.begin();
	has_oled = init_rak1921();
	if (!has_oled)
	{
		Serial.println("No OLED found");
	}
	else
	{
		sprintf(line_str, "RAK Signal Meter - B %.2fV", api.system.bat.get());
		rak1921_write_header(line_str);
	}

	// Setup callbacks and timers
	if (lorawan_mode)
	{
		if (api.lorawan.linkcheck.get() == 0)
		{
			use_link_check = false;
		}
		else
		{
			use_link_check = true;
		}
		api.lorawan.registerRecvCallback(recv_cb_lpw);
		api.lorawan.registerSendCallback(send_cb_lpw);
		api.lorawan.registerJoinCallback(join_cb_lpw);

		if (use_link_check)
		{
			api.lorawan.cfm.set(false);
			api.lorawan.linkcheck.set(2);
			api.lorawan.registerLinkCheckCallback(linkcheck_cb_lpw);
		}
		else
		{
			api.lorawan.cfm.set(true);
			api.lorawan.linkcheck.set(0);
		}

		api.system.timer.create(RAK_TIMER_0, send_packet, RAK_TIMER_PERIODIC);
		api.system.timer.start(RAK_TIMER_0, 15000, NULL);
		api.system.timer.create(RAK_TIMER_1, handle_display, RAK_TIMER_ONESHOT);
	}
	else
	{
		api.system.timer.create(RAK_TIMER_1, handle_display, RAK_TIMER_ONESHOT);
		api.lora.registerPRecvCallback(recv_cb_p2p);
		api.lora.precv(65533);
	}

	// If LoRaWAN, start join if required
	if (lorawan_mode)
	{
		if (!api.lorawan.njs.get())
		{
			api.lorawan.join();
		}
	}
	Serial.println("Start testing");
	api.system.lpm.set(1);
}

/**
 * @brief Loop (unused)
 *
 */
void loop(void)
{
	api.system.sleep.all();
}