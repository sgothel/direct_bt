/*
 * Author: Petre Eftime <petre.p.eftime@intel.com>
 * Copyright (c) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.BTNotification;
import org.direct_bt.HCIStatusCode;

class ValueNotification implements BTNotification<byte[]> {

    @Override
    public void run(final byte[] tempRaw) {
            System.out.print("Temp raw = {");
            for (final byte b : tempRaw) {
                System.out.print(String.format("%02x,", b));
            }
            System.out.print("}");

            /*
             * The temperature service returns the data in an encoded format which can be found in the wiki. Convert the
             * raw temperature format to celsius and print it. Conversion for object temperature depends on ambient
             * according to wiki, but assume result is good enough for our purposes without conversion.
             */
            final int objectTempRaw = (tempRaw[0] & 0xff) | (tempRaw[1] << 8);
            final int ambientTempRaw = (tempRaw[2] & 0xff) | (tempRaw[3] << 8);

            final float objectTempCelsius = Notification.convertCelsius(objectTempRaw);
            final float ambientTempCelsius = Notification.convertCelsius(ambientTempRaw);

            System.out.println(
                    String.format(" Temp: Object = %fC, Ambient = %fC", objectTempCelsius, ambientTempCelsius));


    }

}

class ConnectedNotification implements BTNotification<Boolean> {

    @Override
    public void run(final Boolean connected) {
            System.out.println("Connected");
    }

}

public class Notification {
    // private static final float SCALE_LSB = 0.03125f;
    static boolean running = true;

    static void printDevice(final BTDevice device) {
        System.out.print("Address = " + device.getAddressAndType());
        System.out.print(" Name = " + device.getName());
        System.out.print(" Connected = " + device.getConnected());
        System.out.println();
    }

    static float convertCelsius(final int raw) {
        return raw / 128f;
    }

    /*
     * This program connects to a TI SensorTag 2.0 and reads the temperature characteristic exposed by the device over
     * Bluetooth Low Energy. The parameter provided to the program should be the MAC address of the device.
     *
     * A wiki describing the sensor is found here: http://processors.wiki.ti.com/index.php/CC2650_SensorTag_User's_Guide
     *
     * The API used in this example is based on TinyB v0.3, which only supports polling, but v0.4 will introduce a
     * simplied API for discovering devices and services.
     */
    public static void main(final String[] args) throws InterruptedException {

        if (args.length < 1) {
            System.err.println("Run with <device_address> argument");
            System.exit(-1);
        }

        /*
         * To start looking of the device, we first must initialize the TinyB library. The way of interacting with the
         * library is through the BluetoothManager. There can be only one BluetoothManager at one time, and the
         * reference to it is obtained through the getBluetoothManager method.
         */
        final BTManager manager;
        try {
            manager = BTFactory.getDBusBTManager();
        } catch (BTException | NoSuchMethodException | SecurityException
                | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | ClassNotFoundException e) {
            System.err.println("Failed to initialized "+BTFactory.DBusImplementationID);
            throw new RuntimeException(e);
        }

        /*
         * The manager will try to initialize a BluetoothAdapter if any adapter is present in the system. To initialize
         * discovery we can call startDiscovery, which will put the default adapter in discovery mode.
         */
        @SuppressWarnings("deprecation")
        final boolean discoveryStarted = manager.startDiscovery();

        System.out.println("The discovery started: " + (discoveryStarted ? "true" : "false"));

        /*
         * After discovery is started, new devices will be detected. We can find the device we are interested in
         * through the manager's find method.
         */
        final BTDevice sensor = manager.find(null, args[0], null, 10000);

        if (sensor == null) {
            System.err.println("No sensor found with the provided address.");
            System.exit(-1);
        }

        sensor.enableConnectedNotifications(new ConnectedNotification());

        System.out.print("Found device: ");
        printDevice(sensor);

        if ( HCIStatusCode.SUCCESS == sensor.connect() )
            System.out.println("Sensor with the provided address connected");
        else {
            System.out.println("Could not connect device.");
            System.exit(-1);
        }

        /*
         * After we find the device we can stop looking for other devices.
         */
        //manager.stopDiscovery();

        final Lock lock = new ReentrantLock();
        final Condition cv = lock.newCondition();

        Runtime.getRuntime().addShutdownHook(new Thread() {
            @Override
            public void run() {
                running = false;
                lock.lock();
                try {
                    cv.signalAll();
                } finally {
                    lock.unlock();
                }
            }
        });

        /*
         * Our device should expose a temperature service, which has a UUID we can find out from the data sheet. The service
         * description of the SensorTag can be found here:
         * http://processors.wiki.ti.com/images/a/a8/BLE_SensorTag_GATT_Server.pdf. The service we are looking for has the
         * short UUID AA00 which we insert into the TI Base UUID: f000XXXX-0451-4000-b000-000000000000
         */
        final BTGattService tempService = sensor.find( "f000aa00-0451-4000-b000-000000000000");

        if (tempService == null) {
            System.err.println("This device does not have the temperature service we are looking for.");
            sensor.disconnect();
            System.exit(-1);
        }
        System.out.println("Found service " + tempService.getUUID());

        final BTGattChar tempValue = tempService.find("f000aa01-0451-4000-b000-000000000000");
        final BTGattChar tempConfig = tempService.find("f000aa02-0451-4000-b000-000000000000");
        final BTGattChar tempPeriod = tempService.find("f000aa03-0451-4000-b000-000000000000");

        if (tempValue == null || tempConfig == null || tempPeriod == null) {
            System.err.println("Could not find the correct characteristics.");
            sensor.disconnect();
            System.exit(-1);
        }

        System.out.println("Found the temperature characteristics");

        /*
         * Turn on the Temperature Service by writing 1 in the configuration characteristic, as mentioned in the PDF
         * mentioned above. We could also modify the update interval, by writing in the period characteristic, but the
         * default 1s is good enough for our purposes.
         */
        final byte[] config = { 0x01 };
        tempConfig.writeValue(config, false /* withResponse */);

        final byte[] period = { 100 };
        tempPeriod.writeValue(period, false /* withResponse */);

        tempValue.enableValueNotifications(new ValueNotification());

        lock.lock();
        try {
            while(running)
                cv.await();
        } finally {
            lock.unlock();
        }
        sensor.disconnect();

    }
}
