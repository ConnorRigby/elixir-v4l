defmodule MJPGStreamer do
  use GenServer

  def start_link(args) do
    GenServer.start_link(__MODULE__, args)
  end

  def init(_args) do
    send(self(), :open_port)
    {:ok, %{port: nil}}
  end

  def handle_info(:open_port, state) do
    exe = Application.app_dir(:mjpg_streamer, ["priv", "webcam_port"])

    port =
      :erlang.open_port(
        {:spawn_executable, exe},
        [:binary, {:packet, 4}, {:args, []}, :nouse_stdio, :exit_status]
      )

    {:noreply, %{state | port: port}}
  end

  def handle_info({port, {:data, data}}, %{port: port} = state) do
    frame = :erlang.binary_to_term(data)
    File.write!("stream.jpg", frame)
    {:noreply, state}
  end

  def handle_info({port, {:exit_status, status}}, %{port: port} = state) do
    {:stop, {:port_close, status}, state}
  end
end
