using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

public class ImageObject
{
    public UInt16[] ImageData{ get; set; }
    public Guid ImageId{ get; set; }
    public string SystemId{ get; set; }
    public DateTime ScannedTime{ get; set; }
}

public class FileHelper
{
    public static string[] GetFilesFromFolder(string folderPath)
    {
        if (Directory.Exists(folderPath))
        {
            return Directory.GetFiles(folderPath);
        }
        else
        {
            throw new DirectoryNotFoundException($"The folder '{folderPath}' does not exist.");
        }
    }
}


class ClientApp
{
    /* ------------------------------------------------------------------------ *\
     * client: Manages the TCP connection to the server.
     * stream: Used to send and receive data.
     * paused: A flag that tells the image sender loop whether to pause (based on "PAUSE" command from the server).
     * random: Used to generate scan delays and dummy image data.
    \* ------------------------------------------------------------------------ */

    static TcpClient client;
    static NetworkStream stream;
    static bool paused = false;
    static readonly Random random = new Random();

    static string strClientNum = "cm000";

    static int iSelectFile = 0;
    static int iNoOfFiles = 0;
    static string[] fileNamesToSend = Array.Empty<string>();
    static bool sbKeepConnection = true;

    static async Task Main(string[] args)
    {
        try
        {
            /* ------------------------------------------------------------------------ *\
             * Connects to the server.
             * Starts a background task(ReceiveCommands) to listen for PAUSE / RESUME commands.
             * Starts a loop(StartSendingImages) to periodically send images.
            \* ------------------------------------------------------------------------ */
            string serverIp = "127.0.0.1"; // "192.168.1.1"; // "127.0.0.1";
            int serverPort = 12345;

            Console.WriteLine("USE FORMAT : ClientMachine.exe <ClinetId> <serverIP>");

            if (args.Length >= 2)
            {
                strClientNum = args[0];
                serverIp = args[1];
            }
            else if (args.Length >= 1)
            {
                if (args[0].Contains('.') == false)
                    strClientNum = args[0];
            }
            else
            {
                strClientNum = "cm000";
                serverIp = "127.0.0.1";
            }

            Console.WriteLine($" strClientNum = {strClientNum}   serverIp = {serverIp}");

            // TODO : At the startup (connection establishment), client machine should know whether operator is available to handle data
            client = new TcpClient();
            await client.ConnectAsync(serverIp, serverPort);
            stream = client.GetStream();

            Console.WriteLine("Connected to server.");

            string folderPath = @"E:\01_ClientFiles";
            fileNamesToSend = FileHelper.GetFilesFromFolder(folderPath);
            iNoOfFiles = fileNamesToSend.Length;

            Console.WriteLine($" iNoOfFiles = {iNoOfFiles}");
            foreach (string file in fileNamesToSend)
            {
                Console.WriteLine($"\t {file}");
            }


            // Send identity message
            using (var idStream = new MemoryStream())
            using (var bw = new BinaryWriter(idStream))
            {
                bw.Write((byte)0x01); // Role = Client
                byte[] systemIdBytes = Encoding.UTF8.GetBytes(strClientNum);
                bw.Write(IPAddress.HostToNetworkOrder(systemIdBytes.Length));
                bw.Write(systemIdBytes);
                bw.Write((byte)'\n'); // optional delimiter for legacy support

                await stream.WriteAsync(idStream.ToArray());
            }


            // Start receiving commands from server
            _ = Task.Run(ReceiveCommands);

            // Start sending scanned images
            await StartSendingImages();
        }
        catch (Exception  ex)
        {
            Console.WriteLine(value: $" EXCEPTION : Main() : {ex.Message}");
        }
    }


    // Define image object serialization
    public static byte[] SerializeImageObject(Guid imageId, string systemId, DateTime scannedTime, ushort[] imageData)
    {
        using var ms = new MemoryStream();
        using var bw = new BinaryWriter(ms);

        byte[] idBytes = imageId.ToByteArray();
        byte[] systemIdBytes = Encoding.UTF8.GetBytes(systemId);
        long timestamp = scannedTime.ToBinary(); // efficient format

        // Write in custom order
        bw.Write(idBytes.Length); bw.Write(idBytes);
        bw.Write(systemIdBytes.Length); bw.Write(systemIdBytes);
        bw.Write(timestamp);
        bw.Write(imageData.Length);
        foreach (ushort val in imageData)
            bw.Write(val);

        return ms.ToArray();
    }

    static async Task StartSendingImages()
    {
        int iTaskCnt = 0;
        
        while (sbKeepConnection)
        {
            try
            {
                /* ------------------------------------------------------------------------ *\
                 * Checks if paused: if true, sleeps 1 second and rechecks.
                 * Waits for a random delay(1000 to 6000 ms).
                 * Creates a new ImageObject.
                 * Serializes it to a newline - terminated JSON string.
                 * Sends the JSON string to the server over TCP.
                \* ------------------------------------------------------------------------ */

                if (paused)
                {
                    Console.WriteLine("Paused by server. Waiting...");
                    await Task.Delay(1000);
                }
                {
                    Console.WriteLine("NOT paused. Waiting...");

                    ////int delay = random.Next(1000, 6001); // 1 to 6 seconds
                    int delay = random.Next(5000, 8001); // 5 to 8 seconds
                    await Task.Delay(delay);

                    var imageObj = new ImageObject
                    {
                        ImageId = Guid.NewGuid(),
                        ////SystemId = Environment.MachineName,
                        SystemId = strClientNum,
                        ScannedTime = DateTime.UtcNow,
                        ImageData = GenerateDummyImageData()
                    };

                    if (imageObj.ImageData == null)
                        throw new Exception("Image is not generated : null");

                    Console.WriteLine("Before stream.WriteAsync()...");


                    //////string json = JsonSerializer.Serialize(imageObj, new JsonSerializerOptions { WriteIndented = false });
                    ////////////string json = JsonSerializer.Serialize(imageObj);
                    //////////Console.WriteLine($"{json}");

                    //////byte[] data = Encoding.UTF8.GetBytes(json + "\n");
                    //////await stream.WriteAsync(data, 0, data.Length);



                    client.NoDelay = true;  // disable Nagle
                    //////byte[] buffer = SerializeImageObject(imageObj.ImageId, imageObj.SystemId, imageObj.ScannedTime, imageObj.ImageData); // from above
                    //////await stream.WriteAsync(buffer, 0, buffer.Length);





                    // Serialize image to binary
                    byte[] payload = SerializeImageObject(imageObj.ImageId, imageObj.SystemId, imageObj.ScannedTime, imageObj.ImageData);

                    // Add 4-byte length prefix in network byte order
                    byte[] lengthPrefix = BitConverter.GetBytes(IPAddress.HostToNetworkOrder(payload.Length));

                    // Combine length + payload
                    byte[] fullMessage = new byte[4 + payload.Length];
                    Buffer.BlockCopy(lengthPrefix, 0, fullMessage, 0, 4);
                    Buffer.BlockCopy(payload, 0, fullMessage, 4, payload.Length);

                    // Send it
                    var sw = Stopwatch.StartNew();
                    await stream.WriteAsync(fullMessage, 0, fullMessage.Length);
                    sw.Stop();

                    Console.WriteLine($"{strClientNum} => Sent image: {imageObj.ImageId}, Time: {sw.ElapsedMilliseconds} ms, Size: {fullMessage.Length} bytes");





                    Console.WriteLine($"{strClientNum} : {++iTaskCnt} => Sent image: {imageObj.ImageId}, {imageObj.SystemId}, {imageObj.ScannedTime}, delay: {delay}ms, []");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(value: $" EXCEPTION : StartSendingImages() : {ex.Message}");
                Console.WriteLine($" Stopping I/O with server. Reestablish the connection");
                sbKeepConnection = false;
                paused = true;
            }
        }
    }

    static async Task ReceiveCommands()
    {
        try
        {
            /* ------------------------------------------------------------------------ *\
             * Continuously reads lines from the server.
             * If the line is "PAUSE", sets paused = true.
             * If the line is "RESUME", sets paused = false.
            \* ------------------------------------------------------------------------ */

            using var reader = new StreamReader(stream, Encoding.UTF8);
            while (sbKeepConnection)
            {
                ////Console.WriteLine($"ReceiveCommands from server waiting .....");

                var line = await reader.ReadLineAsync();
                if (line != null)
                {
                    Console.WriteLine();
                    Console.WriteLine($" {Environment.NewLine}***** 1. ReceiveCommands() from server: line = {line}");
                    Console.WriteLine();
                    if (line == "PAUSE")
                    {
                        paused = true;
                    }
                    else if (line == "RESUME")
                    {
                        paused = false;
                    }
                    //////Console.WriteLine($" ***** 2. ReceiveCommands() from server: paused = {paused}");
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : ReceiveCommands() : {ex.Message}");
            Console.WriteLine($" Stopping I/O with server. Reestablish the connection");
            sbKeepConnection = false;
        }
    }

    static int[]? GenerateDummyImageData_V1(int size)
    {
        try
        {
            /* ------------------------------------------------------------------------ *\
             * Creates an array of random int values between 0 and 255.
             * Simulates raw image pixel data.
            \* ------------------------------------------------------------------------ */

            var data = new int[size];
            for (int i = 0; i < size; i++)
                data[i] = random.Next(0, 256);
            return data;
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : GenerateDummyImageData_V1() : {ex.Message}");
            throw;
        }
    }

    static bool bDoOnlyOnce = true;
    static int iCols = 0, iRows = 0;
    static UInt16[]? GenerateDummyImageData()
    {
        try
        {
            UInt16[] data = Array.Empty<ushort>();

            StreamReader sr = new StreamReader(fileNamesToSend[iSelectFile%iNoOfFiles]);
            Console.Write($" File Reading : {fileNamesToSend[iSelectFile%iNoOfFiles]}");

            iSelectFile++;
            //Read the first line of text
            string? line = sr.ReadLine();     //0th index has Width/iCols //1st index has Height/Rows allcoate data array of size Width*Height        
	        int i = 0;

            //Continue to read until you reach end of file
            while (line != null)
            {
                string[] Words = line.Split('\t');
                if (bDoOnlyOnce == true)
                {
                    //0th index has Width/iCols //1st index has Height/Rows allcoate data array of size Width*Height

                    iCols = UInt16.Parse(Words[0]);
                    iRows = UInt16.Parse(Words[1]);

                    data = new UInt16[iCols * iRows];

                    bDoOnlyOnce = false;
                }

                for (int index = 0; index < iCols; index++)
                    data[iCols * i + index] = UInt16.Parse(Words[index]);

                //Read the next line
                line = sr.ReadLine();
                i++;
            }
            //close the file
            sr.Close();

            Console.WriteLine($" data: {data[0]}, {data[1]}, {data[2]}, {data[3]}, {data[4]} ");
            bDoOnlyOnce = true;

            return data;
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : GenerateDummyImageData() : {ex.Message}");
            throw;
        }
    }
}
