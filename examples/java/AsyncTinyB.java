import java.util.concurrent.locks.*;

import org.direct_bt.BTDevice;
import org.direct_bt.BTException;
import org.direct_bt.BTFactory;
import org.direct_bt.BTGattChar;
import org.direct_bt.BTGattService;
import org.direct_bt.BTManager;
import org.direct_bt.HCIStatusCode;

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.TimeUnit;

public class AsyncTinyB {
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

        /*
         * After we find the device we can stop looking for other devices.
         */
        try {
            manager.stopDiscovery();
        } catch (final BTException e) {
            System.err.println("Discovery could not be stopped right now");
        }

        if (sensor == null) {
            System.err.println("No sensor found with the provided address.");
            System.exit(-1);
        }

        System.out.print("Found device: ");
        printDevice(sensor);

        if ( HCIStatusCode.SUCCESS == sensor.connect() )
            System.out.println("Sensor with the provided address connected");
        else {
            System.out.println("Could not connect device.");
            System.exit(-1);
        }

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

        /*
         * Each second read the value characteristic and display it in a human readable format.
         */
        while (running) {
            final byte[] tempRaw = tempValue.readValue();
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

            final float objectTempCelsius = convertCelsius(objectTempRaw);
            final float ambientTempCelsius = convertCelsius(ambientTempRaw);

            System.out.println(
                    String.format(" Temp: Object = %fC, Ambient = %fC", objectTempCelsius, ambientTempCelsius));

            lock.lock();
            try {
                cv.await(1, TimeUnit.SECONDS);
            } finally {
                lock.unlock();
            }
        }
        sensor.disconnect();

    }
}
