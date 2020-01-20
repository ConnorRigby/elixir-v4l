defmodule MJPGStreamer.MixProject do
  use Mix.Project

  def project do
    [
      app: :mjpg_streamer,
      version: "0.1.0",
      elixir: "~> 1.9",
      start_permanent: Mix.env() == :prod,
      build_embedded: true,
      deps: deps(),
      compilers: [:elixir_make | Mix.compilers()]
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger, :runtime_tools]
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      # Dependencies for all targets
      {:elixir_make, "~> 0.6", runtime: false}
    ]
  end
end
