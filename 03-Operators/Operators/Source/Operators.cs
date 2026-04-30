using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.Timers;
using OpenCvSharp;
using System.Runtime.InteropServices;
using static System.Runtime.InteropServices.JavaScript.JSType;
using System.Diagnostics;
using System.IO;
using System.Collections.Generic;



public class ImageObject
{
    public string ImageId { get; set; }
    public string SystemId { get; set; }
    public string ScannedTime { get; set; }
    public List<UInt16> ImageData { get; set; }
}

public class DecisionResult
{
    public string ImageId { get; set; }
    public string OperatorId { get; set; }
    public string Decision { get; set; } // "Accepted" or "Rejected"
    public string DecisionTime { get; set; }
}


class OperatorApp
{
    static TcpClient client;
    static NetworkStream stream;
    const int t1 = 2000; // milliseconds allowed for decision
    static string strOperatorId = "op000";
    static bool sbKeepConnection = true;

    static async Task Main(string[] args)
    {
        try
        {
            string serverIp = "127.0.0.1"; // "192.168.1.1"; // "127.0.0.1";
            int serverPort = 12345;

            Console.WriteLine("USE FORMAT : Operator.exe <opId> <serverIP>");

            if (args.Length >= 2)
            {
                strOperatorId = args[0];
                serverIp = args[1];
            }
            else if (args.Length >= 1)
            {
                if (args[0].Contains('.') == false)
                    strOperatorId = args[0];
            }
            else
            {
                strOperatorId = "op000";
                serverIp = "127.0.0.1";
            }

            client = new TcpClient(serverIp, serverPort);
            client.NoDelay = true;
            stream = client.GetStream();

            Console.WriteLine($" Operator {strOperatorId} connected to server {serverIp} with port {serverPort}");
            Console.WriteLine($" client.ReceiveBufferSize = {client.ReceiveBufferSize}     client.SendBufferSize = {client.SendBufferSize}");

            // Send identity message
            using var idStream = new MemoryStream();
            using var bw = new BinaryWriter(idStream);

            bw.Write((byte)0x02); // Role = Operator
            byte[] systemIdBytes = Encoding.UTF8.GetBytes(strOperatorId);
            bw.Write(IPAddress.HostToNetworkOrder(systemIdBytes.Length));
            bw.Write(systemIdBytes);
            bw.Write((byte)'\n'); // optional delimiter for backward compat

            await stream.WriteAsync(idStream.ToArray());


            Console.WriteLine($"After Hankshake() {strOperatorId}");

            while (sbKeepConnection)
            {
                byte[] imgPayload = await ReadBinaryMessageAsync();
                ImageObject image = DeserializeImage(imgPayload);

                Console.WriteLine($"{image.SystemId}] ImageId: {image.ImageId} ScannedTime: {image.ScannedTime}");
                Console.WriteLine($"ImageData: {image.ImageData[0]}, {image.ImageData[1]}, {image.ImageData[2]}, {image.ImageData[3]}, {image.ImageData[4]}");

                SaveGreyImage(image.ImageData.ToArray(), image.ImageData[1], image.ImageData[0], strOperatorId);

                // Decision logic
                var decision = await GetDecisionAsync(image);

                // Send result to server
                byte[] decPayload = SerializeDecision(decision);
                byte[] full = WrapWithLengthPrefix(decPayload);
                await stream.WriteAsync(full, 0, full.Length);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : Main() : {ex.Message}");
            Console.WriteLine($" Stopping I/O with server. Reestablish the connection");
            sbKeepConnection = false;
        }
    }

    static int id = 0;

    public static void SaveGreyImage(ushort[] data, int irows, int icols, string strOpId)
    {
        try
        {
            if (data.Length != irows * icols)
                throw new ArgumentException("Data length does not match rows * cols.");

            string directory = $"E:\\02_OperatorFiles\\";
            Directory.CreateDirectory(directory); // Ensure the directory exists

            string fileName = Path.Combine(directory, $"Grey_{strOpId}_{id}.png");

            // Create empty Mat
            using var grey = new Mat(irows, icols, MatType.CV_16UC1);

            // Use the correct overload to copy the full buffer
            grey.SetArray(data);

            // Save the image
            Cv2.ImWrite(fileName, grey);

            id++;
        }
        catch (Exception ex) 
        {
            Console.WriteLine(value: $" EXCEPTION : SaveGreyImage() : {ex.Message}");
            throw;
        }
    }

    static async Task<string> ReadMessageAsync()
    {
        try
        {
            byte[] buffer = new byte[4096];
            StringBuilder sb = new StringBuilder();
            int bytesRead;

            Console.WriteLine($" Before : ReadMessageAsync() :: stream.ReadAsync() : ");

            do
            {
                bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length);
                sb.Append(Encoding.UTF8.GetString(buffer, 0, bytesRead));
            }
            while (!sb.ToString().Contains("\n"));

            string message = sb.ToString().Trim();
            return message;
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : ReadMessageAsync() : {ex.Message}");
            throw;
        }
    }

    static async Task<byte[]> ReadBinaryMessageAsync()
    {
        byte[] lenBytes = new byte[4];
        await stream.ReadAsync(lenBytes, 0, 4);
        int len = IPAddress.NetworkToHostOrder(BitConverter.ToInt32(lenBytes, 0));

        byte[] buffer = new byte[len];
        int read = 0;
        while (read < len)
        {
            int n = await stream.ReadAsync(buffer, read, len - read);
            if (n == 0) throw new IOException("Stream closed.");
            read += n;
        }
        return buffer;
    }


    static async Task<DecisionResult> GetDecisionAsync(ImageObject image)
    {
        try
        {
            var decisionTask = Task.Run(async () =>
            {
                Console.WriteLine("[Press A to accept, R to reject within time]");

                while (true)
                {
                    if (Console.KeyAvailable)
                    {
                        var key = Console.ReadKey(true).Key;
                        if (key == ConsoleKey.A) return "Accepted";
                        if (key == ConsoleKey.R) return "Rejected";
                    }

                    await Task.Delay(10);
                }
            });

            var timeoutTask = Task.Delay(t1);
            var completed = await Task.WhenAny(decisionTask, timeoutTask);
            string decision = completed == decisionTask ? decisionTask.Result : "Rejected";

            Console.WriteLine($"Decision: {decision}");

            return new DecisionResult
            {
                ImageId = image.ImageId,
                OperatorId = strOperatorId,
                Decision = decision,
                DecisionTime = DateTime.UtcNow.ToString("o")
            };
        }
        catch (Exception ex)
        {
            Console.WriteLine($" EXCEPTION : GetDecisionAsync() : {ex.Message}");
            throw;
        }
    }


    static ImageObject DeserializeImage(byte[] buf)
    {
        try
        {

            using var ms = new MemoryStream(buf);
            using var br = new BinaryReader(ms);

            int idLen = br.ReadInt32();
            Guid imageId = new Guid(br.ReadBytes(idLen));

            int sysLen = br.ReadInt32();
            string systemId = Encoding.UTF8.GetString(br.ReadBytes(sysLen));

            long ticks = br.ReadInt64();
            DateTime scanned = DateTime.FromBinary(ticks);

            int count = br.ReadInt32();
            List<ushort> imageData = new List<ushort>(count);
            for (int i = 0; i < count; i++)
                imageData.Add(br.ReadUInt16());

            return new ImageObject { ImageId = imageId.ToString(), SystemId = systemId, ScannedTime = scanned.ToString("o"), ImageData = imageData };
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : DeserializeImage() : {ex.Message}");
            throw;
        }
    }

    static readonly MemoryStream msDecision = new MemoryStream(1024); // Static reusable buffer
    static readonly BinaryWriter bwDecision = new BinaryWriter(msDecision);

    static byte[] SerializeDecision(DecisionResult result)
    {
        try
        {
            msDecision.SetLength(0); // Reuse buffer

            byte[] idBytes = Encoding.UTF8.GetBytes(result.ImageId);
            byte[] opBytes = Encoding.UTF8.GetBytes(result.OperatorId);
            byte[] dBytes = Encoding.UTF8.GetBytes(result.Decision);
            byte[] timeBytes = Encoding.UTF8.GetBytes(result.DecisionTime);

            bwDecision.Write(idBytes.Length); bwDecision.Write(idBytes);
            bwDecision.Write(opBytes.Length); bwDecision.Write(opBytes);
            bwDecision.Write(dBytes.Length); bwDecision.Write(dBytes);
            bwDecision.Write(timeBytes.Length); bwDecision.Write(timeBytes);

            return msDecision.ToArray();
        }
        catch (Exception ex)
        {
            Console.WriteLine($" EXCEPTION : SerializeDecision() : {ex.Message}");
            throw;
        }
    }


    static byte[] WrapWithLengthPrefix(byte[] payload)
    {
        try
        {
            byte[] lenBytes = BitConverter.GetBytes(IPAddress.HostToNetworkOrder(payload.Length));
            byte[] full = new byte[4 + payload.Length];
            Buffer.BlockCopy(lenBytes, 0, full, 0, 4);
            Buffer.BlockCopy(payload, 0, full, 4, payload.Length);
            return full;
        }
        catch (Exception ex)
        {
            Console.WriteLine(value: $" EXCEPTION : WrapWithLengthPrefix() : {ex.Message}");
            throw;
        }
    }
}
