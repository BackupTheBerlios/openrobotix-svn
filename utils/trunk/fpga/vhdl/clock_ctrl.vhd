--------------------------------------------------------------------------------
-- clock-ctrl.vhd - Clock FPGA design of Miniroboter CPU-Board
--
-- Copyright (C) 2010
-- Heinz Nixdorf Institute - University of Paderborn
-- Department of System and Circuit Technology
-- Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de>
--
-- This library is free software; you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published
-- by the Free Software Foundation; either version 3 of the License, or
-- (at your option) any later version.
--
-- This library is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this library. If not, see <http://www.gnu.org/licenses/>
--------------------------------------------------------------------------------
library IEEE;

use IEEE.std_logic_1164.all;
use IEEE.std_logic_unsigned.all;

library unisim;
use unisim.vcomponents.all;
--------------------------------------------------------------------------------
--  ENTITY                                                                    --
--------------------------------------------------------------------------------
entity CLOCK_CTRL is
port (
	SDCLK0			: in std_logic;
	RESET_N			: in std_logic;
	CLK0			: out std_logic;
	MEM_CLK			: out std_logic;
	LOCKED			: out std_logic
	);
end CLOCK_CTRL;

--------------------------------------------------------------------------------
--  ARCHITECTURE                                                              --
--------------------------------------------------------------------------------
architecture RTL of CLOCK_CTRL is

signal SDCLK0_int		: std_logic;
signal MEM_CLK_int		: std_logic;
signal CLK0_bu			: std_logic;
signal MEM_CLK_bu		: std_logic;
signal reset			: std_logic;
signal locked_int		: std_logic;
signal status			: std_logic_vector(7 downto 0);

attribute clock_signal : string;
attribute clock_signal of MEM_CLK_bu : signal is "yes";

attribute clk_feedback : string;
attribute clk_feedback of DCM_1 : label is "NONE";
attribute clkfx_divide : integer;
attribute clkfx_divide of DCM_1 : label is 1;
--attribute clkin_period : real;
--attribute clkin_period of DCM_1 : label is 20.0;
attribute clkfx_multiply : integer;
attribute clkfx_multiply of DCM_1 : label is 2;

--------------------------------------------------------------------------------
begin
--------------------------------------------------------------------------------

	reset <= not RESET_N;

	ibufg_0 : ibufg
	port map (
		O	=>	SDCLK0_int,
		I	=>	SDCLK0
	);

	bufg_0 : bufg
	port map (
		O	=>	CLK0_bu,
		I	=>	SDCLK0_int
	);

	bufg_mem : bufg
	port map (
		O	=>	MEM_CLK_bu,
		I	=>	MEM_CLK_int
	);

	DCM_1 : DCM_SP
	generic map (
--		CLKIN_PERIOD		=> 20,
		CLK_FEEDBACK 		=> "NONE",
		CLKFX_DIVIDE 		=> 1,
		CLKFX_MULTIPLY 		=> 2
	)
	port map (
		CLK0		=>	open,
		CLK180 		=>	open,
		CLK270 		=>	open,
		CLK2X 		=>	open,
		CLK2X180	=>	open,
		CLK90 		=>	open,
		CLKDV 		=>	open,
		CLKFX 		=>	MEM_CLK_int,
		CLKFX180 	=>	open,
		LOCKED		=>	locked_int,
		PSDONE		=>	open,
		STATUS		=>	status,
		CLKFB		=>	'0',
		CLKIN		=>	SDCLK0_int,
		DSSEN		=>	'0',
		PSCLK		=>	'0',
		PSEN		=>	'0',
		PSINCDEC	=>	'0',
		RST		=>	reset
	);

	CLK0 <= CLK0_bu;
	MEM_CLK <= MEM_CLK_bu;
	LOCKED <= locked_int and not status(2);

end RTL;
