using NAudio.CoreAudioApi;
using NAudio.Wave;
using System.Text.RegularExpressions;

namespace AudioPassthrough {

    public partial class Form1 : Form {
        private MMDevice device = null;
        private MMDevice mmdeviceplayback;
        private WasapiOut hapticStream;
        private WasapiOut audioPassthroughStream;
        private WasapiOut hapticsToSpeakerStream;
        private WasapiLoopbackCapture wasapiLoopbackCapture = null;
        private MMDeviceEnumerator mmdeviceEnumerator = new MMDeviceEnumerator();
        private bool StartNewPlayback = true;
        private BufferedWaveProvider audioPassthroughBuffer = null;
        private float speakerPlaybackVolume = 0;
        private float leftActuatorVolume = 1;
        private float rightActuatorVolume = 1;
        private bool SystemAudioPlayback = true;
        private string audioID = string.Empty;
        private string arg = string.Empty;
        private MMDevice currentPlaybackDevice;

        public Form1(string[] args) {
            InitializeComponent();
            if (args.Length == 0) {
                MessageBox.Show("No arguments!");
                Environment.Exit(0);
            }
            arg = args[0];
        }

        private void Form1_Load(object sender, EventArgs e) {
            new Thread(LookForNewDevice).Start();
            StartAudioToHaptics(arg.ToLower());
        }

        private void LookForNewDevice() {
            MMDeviceEnumerator enumerator = new MMDeviceEnumerator();
            currentPlaybackDevice = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            Thread.Sleep(5000);
            bool disconnected = false;

            while (true) {
                MMDevice newDevice = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);

                device = null;

                foreach (MMDevice mmdevice in mmdeviceEnumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active)) {
                    try {                    
                        if (mmdevice.FriendlyName.ToLower() == arg.ToLower()) {
                            device = mmdevice;
                            break;
                        }
                    }
                    catch {
                        continue;
                    }
                }

                if (device == null) {
                    disconnected = true;
                }
                else if (device != null && disconnected) {
                    StartAudioToHaptics(arg.ToLower(), true);
                    disconnected = false;
                }

                if (newDevice.State == DeviceState.Active && newDevice.FriendlyName != currentPlaybackDevice.FriendlyName && !newDevice.FriendlyName.Contains("Wireless Controller")) {
                    currentPlaybackDevice = newDevice;
                    StartAudioToHaptics(arg.ToLower());
                }
                Thread.Sleep(3000);
            }
        }

        private void StartAudioToHaptics(string instanceID = "", bool reconnectattempt = false) {
            device = null;
            foreach (MMDevice mmdevice in mmdeviceEnumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active)) {
                try {
                    //MessageBox.Show(mmdevice.FriendlyName.ToLower() + " | " + arg.ToLower());
                    if (mmdevice.FriendlyName.ToLower() == arg.ToLower()) {
                        device = mmdevice;
                        audioID = instanceID;
                        break;
                    }
                }
                catch {
                    continue;
                }
            }

            if (reconnectattempt == false) // Return if both failed
            {
                if (device == null || device.State == DeviceState.NotPresent || device.State == DeviceState.Unplugged || device.State == DeviceState.Disabled) {
                    MessageBox.Show("Couldn't find DualSense Wireless Controller Speaker!");
                    Environment.Exit(0);
                }
            }
            else {
                if (device == null || device.State == DeviceState.NotPresent || device.State == DeviceState.Unplugged || device.State == DeviceState.Disabled) {
                    return;
                }
            }

            // AUDIO PASSTHROUGH STREAM
            setNewPlayback();
            audioPassthroughStream = new WasapiOut(device, AudioClientShareMode.Shared, true, 10);
            audioPassthroughBuffer = new BufferedWaveProvider(WaveFormat.CreateCustomFormat(WaveFormatEncoding.IeeeFloat, wasapiLoopbackCapture.WaveFormat.SampleRate, wasapiLoopbackCapture.WaveFormat.Channels, wasapiLoopbackCapture.WaveFormat.AverageBytesPerSecond, wasapiLoopbackCapture.WaveFormat.BlockAlign, wasapiLoopbackCapture.WaveFormat.BitsPerSample));
            audioPassthroughBuffer.BufferLength = 5000000; // 5MB buffer
            audioPassthroughBuffer.ReadFully = true;
            audioPassthroughBuffer.DiscardOnBufferOverflow = true;

            MultiplexingWaveProvider multiplexingWaveProviderAP = new MultiplexingWaveProvider(new BufferedWaveProvider[] {
                    audioPassthroughBuffer,}, 4);

            multiplexingWaveProviderAP.ConnectInputToOutput(0, 0);
            multiplexingWaveProviderAP.ConnectInputToOutput(1, 1);
            multiplexingWaveProviderAP.ConnectInputToOutput(0, 2);
            multiplexingWaveProviderAP.ConnectInputToOutput(1, 3);
            audioPassthroughStream.Init(multiplexingWaveProviderAP);
            audioPassthroughStream.Play();

            trackBar1.Invoke((MethodInvoker)delegate {
                if (trackBar1.Value != 0) {
                    setVolume((float)(Convert.ToDouble(trackBar1.Value) / 100), 1, 1);
                }
                else {
                    setVolume(0, 1, 1);
                }
            });
        }

        private void setNewPlayback() {
            if (wasapiLoopbackCapture != null) {
                wasapiLoopbackCapture.StopRecording();
                wasapiLoopbackCapture.Dispose();
            }

            mmdeviceplayback = mmdeviceEnumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            wasapiLoopbackCapture = new WasapiLoopbackCapture(mmdeviceplayback);
            wasapiLoopbackCapture.DataAvailable += WasapiLoopbackCapture_DataAvailable;
            wasapiLoopbackCapture.StartRecording();
        }

        private void WasapiLoopbackCapture_DataAvailable(object? sender, WaveInEventArgs e) {
            if (SystemAudioPlayback) {
                // Define your gain factor (adjust as needed)
                float gainFactor = trackBar2.Value; // Example to double the volume

                // Process the audio buffer
                int sampleCount = e.BytesRecorded / sizeof(float); // Number of samples
                for (int i = 0; i < sampleCount; i++) {
                    float sample = BitConverter.ToSingle(e.Buffer, i * sizeof(float));

                    sample *= gainFactor;

                    if (sample > 1.0f) sample = 1.0f;
                    if (sample < -1.0f) sample = -1.0f;

                    byte[] modifiedSample = BitConverter.GetBytes(sample);
                    Buffer.BlockCopy(modifiedSample, 0, e.Buffer, i * sizeof(float), sizeof(float));
                }

                audioPassthroughBuffer.AddSamples(e.Buffer, 0, e.BytesRecorded);
            }
        }

        private void setVolume(float speaker, float left, float right) {
            speakerPlaybackVolume = speaker;
            leftActuatorVolume = left;
            rightActuatorVolume = right;

            try {
                if (audioPassthroughStream != null) {
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(0, speakerPlaybackVolume);
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(1, speakerPlaybackVolume);
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(2, leftActuatorVolume);
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(3, rightActuatorVolume);
                }
            }
            catch (ArgumentOutOfRangeException) {
                throw new ArgumentOutOfRangeException("Volume must be between 0.0 and 1.0.");
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e) {
            if (hapticStream != null) {
                hapticStream.Dispose();
            }

            if (audioPassthroughStream != null) {
                audioPassthroughStream.Dispose();
                audioPassthroughBuffer.ClearBuffer();
            }

            if (wasapiLoopbackCapture != null) {
                wasapiLoopbackCapture.Dispose();
            }

            Environment.Exit(0);
        }

        private void trackBar1_Scroll(object sender, EventArgs e) {
            if (trackBar1.Value == 0) {
                setVolume(0, 1, 1);
            }
            else {
                setVolume((float)(Convert.ToDouble(trackBar1.Value) / 100), 1, 1);
            }
        }

        private void button1_Click(object sender, EventArgs e) {
            this.WindowState = FormWindowState.Minimized;
        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e) {
            if (hapticStream != null) {
                hapticStream.Dispose();
            }

            if (audioPassthroughStream != null) {
                audioPassthroughStream.Dispose();
                audioPassthroughBuffer.ClearBuffer();
            }

            if (wasapiLoopbackCapture != null) {
                wasapiLoopbackCapture.Dispose();
            }

            Environment.Exit(0);
        }
    }
}
