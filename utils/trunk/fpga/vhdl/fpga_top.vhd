--------------------------------------------------------------------------------
-- fpga_tb.vhd - Toplevel FPGA design of Miniroboter CPU-Board
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
use IEEE.std_logic_misc.all;

--------------------------------------------------------------------------------
--  ENTITY                                                                    --
--------------------------------------------------------------------------------
entity FPGA_TOP is
port (
	--Clocks and General (20 signals)
	SDCLK0			: in	std_logic;
--	SDCLK1			: in	std_logic;

--	SYS_CLK			: in	std_logic;

	-- Memory Interface
	DATA			: inout	std_logic_vector(31 downto 0);
	ADDR			: in	std_logic_vector(25 downto 2);
	DQM			: in	std_logic_vector(3 downto 0);
	OE_N			: in	std_logic;
--	WE_N			: in	std_logic;
	PWE_N			: in	std_logic;
--	SDCAS_N			: in	std_logic;
--	RDWR_N			: in	std_logic;
	RDY				: out	std_logic;

	FPGA_CS_N		: in	std_logic;

	-- BUSMASTER
--	SDRAS_N 		: out	std_logic;
--	SDCLKE_N 		: out	std_logic;
--	SDCS0_N 		: out	std_logic;
--	CS0_N 			: out	std_logic;
--	CPLD_CS_N 		: out	std_logic;
--	USB_CS_N 		: out	std_logic;
--	MBGNT 			: in	std_logic;	-- MBREG / DREQ1
--	MBREQ			: out	std_logic;	-- MBGNT / DVAL1

	-- DMA
--	DREQ0			: out	std_logic;
--	DVAL0			: in	std_logic;
--	DREQ1			: out	std_logic;
--	DVAL1			: in	std_logic;	-- MBREG / DREQ1

	-- FPGA
--	FPGA_D	 		: in	std_logic_vector(7 downto 0);
--	FPGA_CCLK 		: in	std_logic;
--	FPGA_INIT_N 		: in	std_logic;
--	FPGA_RDWR_N 		: in	std_logic;
--	FPGA_BUSY 		: out	std_logic;
--	FPGA_CSI_N 		: in	std_logic;

--	FPGA_IO			: inout	std_logic_vector(15 downto 0);
	FPGA_LED_RED_N 		: out	std_logic;
	FPGA_LED_GREEN_N 	: out	std_logic;
--	FPGA_IRQ_N 		: out	std_logic;
	FPGA_RESET_N 		: in	std_logic;

	-- MSL
--	MSL_OB			: in	std_logic_vector(3 downto 0);
--	MSL_OB_CLK		: in	std_logic;
--	MSL_OB_STB		: in	std_logic;
--	MSL_OB_WAIT		: out	std_logic;
--	MSL_IB			: out	std_logic_vector(3 downto 0);
--	MSL_IB_CLK		: out	std_logic;
--	MSL_IB_STB		: out	std_logic;
--	MSL_IB_WAIT		: in	std_logic;

	-- SYS
	SYS_IO			: inout	std_logic_vector(31 downto 0)
--	SYS_STATUS		: in	std_logic;
--	SYS_ERROR		: in	std_logic

	);
end FPGA_TOP;

------------------------------------------------------------------------------
--  ARCHITECTURE                                                            --
------------------------------------------------------------------------------
architecture BEH of FPGA_TOP is

	component CLOCK_CTRL is
	port (
		SDCLK0			: in	std_logic;
		RESET_N			: in	std_logic;
		CLK0			: out	std_logic;
		MEM_CLK			: out	std_logic;
		LOCKED			: out	std_logic
	);
	end component;

	component LOCALBUS is
	port (
		CLK			: in	std_logic;
		RESET_N			: in	std_logic;

		MEM_ADDR		: in	std_logic_vector(25 downto 2);
		MEM_DATA_I		: in	std_logic_vector(31 downto 0);
		MEM_DATA_O		: out	std_logic_vector(31 downto 0);
		MEM_DATA_T_N		: out	std_logic_vector(31 downto 0);
		MEM_DQM			: in	std_logic_vector(3 downto 0);
		MEM_CS_N		: in	std_logic;
		MEM_PWE_N		: in	std_logic;
		MEM_OE_N		: in	std_logic;

		LOC_ADDR		: out	std_logic_vector(25 downto 2);
		LOC_DATA_I		: in	std_logic_vector(31 downto 0);
		LOC_DATA_O		: out	std_logic_vector(31 downto 0);
		LOC_DQM			: out	std_logic_vector(3 downto 0);
		LOC_VALID_N		: out	std_logic;
		LOC_READ_N		: out	std_logic;
		LOC_WRITE_N		: out	std_logic
	);
	end component;

	-- Clock
	signal clk0			: std_logic;
	signal mem_clk			: std_logic;
	signal locked			: std_logic;
	signal reset_n			: std_logic;

	-- Memory Interface
	signal mem_data_i		: std_logic_vector(31 downto 0);
	signal mem_data_o		: std_logic_vector(31 downto 0);
	signal mem_data_t_n		: std_logic_vector(31 downto 0);

	-- Local Interface
	signal loc_addr			: std_logic_vector(25 downto 2);
	signal loc_data_i 		: std_logic_vector(31 downto 0);
	signal loc_data_o 		: std_logic_vector(31 downto 0);
	signal loc_dqm			: std_logic_vector(3 downto 0);
	signal loc_valid_n		: std_logic;
	signal loc_read_n		: std_logic;
	signal loc_write_n		: std_logic;

	signal current_addr		: std_logic_vector(3 downto 2);

	-- IO
	signal current_io_reg	: std_logic_vector(31 downto 0);
	signal current_io_i		: std_logic_vector(31 downto 0);
	signal current_io_o		: std_logic_vector(31 downto 0);
	signal current_io_t		: std_logic_vector(31 downto 0);
	signal current_io_cs_n	: std_logic;
	signal next_io_cs_n		: std_logic;

	-- LED
	signal current_led		: std_logic_vector(25 downto 0);
	
	-- Buffer
	signal current_buffer	: std_logic_vector(31 downto 0);	
	signal current_buffer_cs_n		: std_logic;
	signal next_buffer_cs_n	: std_logic;
	
--------------------------------------------------------------------------------
begin
--------------------------------------------------------------------------------

	RDY <= '1';

	clock : CLOCK_CTRL
	port map (
		SDCLK0	=> SDCLK0,
		RESET_N	=> FPGA_RESET_N,
		CLK0		=> clk0,
		MEM_CLK	=> mem_clk,
		LOCKED	=> locked
	);

	reset_n <= locked and FPGA_RESET_N;

	mem_data_i <= DATA;
	mem_data_loop: for i in 0 to 31 generate
	begin
		DATA(i) <= mem_data_o(i) when (mem_data_t_n(i) = '0') else 'Z';
	end generate mem_data_loop;

	localb : LOCALBUS
	port map (
		CLK => mem_clk,
		RESET_N => reset_n,

		MEM_ADDR => ADDR,
		MEM_DATA_I => mem_data_i,
		MEM_DATA_O => mem_data_o,
		MEM_DATA_T_N => mem_data_t_n,
		MEM_DQM => DQM,
		MEM_CS_N => FPGA_CS_N,
		MEM_PWE_N => PWE_N,
		MEM_OE_N => OE_N,

		LOC_ADDR => loc_addr,
		LOC_DATA_I => loc_data_i,
		LOC_DATA_O => loc_data_o,
		LOC_DQM => loc_dqm,
		LOC_VALID_N	=> loc_valid_n,
		LOC_READ_N => loc_read_n,
		LOC_WRITE_N => loc_write_n
	);

	io_loop: for i in 0 to 31 generate
	begin
		SYS_IO(i) <= current_io_o(i) when (current_io_t(i) = '1') else 'Z';
	end generate io_loop;
	
	-- Chip Select Mux
	cs_mux : process(loc_valid_n, loc_addr, loc_dqm)
	begin
		next_io_cs_n <= '1';
		next_buffer_cs_n <= '1';
		if (loc_valid_n = '0' and loc_addr(25 downto 4) = "00" & X"0000"
				and loc_dqm = "0000") then
			next_io_cs_n <= '0';
		else
			next_buffer_cs_n <= '0';
		end if;
	end process cs_mux;
	
	-- Data Mux
	data_mux : process(current_io_cs_n, current_buffer_cs_n, current_addr,
				current_io_i, current_io_o, current_io_t, current_buffer)
	begin
		loc_data_i <= (others => '0');	
		if (current_io_cs_n = '0') then
			case current_addr is
				when "00" =>	-- in
					loc_data_i <= current_io_i;
				when "01" =>	-- out
					loc_data_i <= current_io_o;
				when "10" =>	-- gpio output
					loc_data_i <= current_io_t;
				when others =>
			end case;
		elsif (current_buffer_cs_n = '0') then
			loc_data_i <= current_buffer;
		end if;
	end process data_mux;
	
	-- Register
	reg: process(mem_clk, reset_n)
	begin
		if (reset_n = '0') then
			current_addr <= (others => '0');
			current_led <= (others => '0');
			current_io_reg <= (others => '0');
			current_io_i <= (others => '0');
			current_io_o <= (others => '0');
			current_io_t <= (others => '0');
			current_io_cs_n <= '1';
			current_buffer <= (others => '0');
			current_buffer_cs_n <= '1';
		elsif (mem_clk'event and mem_clk = '1') then
			if (loc_valid_n = '0') then
				current_addr <= loc_addr(3 downto 2);
			end if;			
			if (loc_write_n = '0') then
				if (current_io_cs_n = '0') then
					case loc_addr(3 downto 2) is
						when "01" =>	-- out
							current_io_o <= loc_data_o;
						when "10" =>	-- gpio output
							current_io_t <= loc_data_o;
						when others =>
					end case;
				elsif (current_buffer_cs_n = '0') then
					current_buffer <= loc_data_o;
				end if;
			end if;
			current_io_reg <= SYS_IO;
			current_io_i <= current_io_reg;
			current_led <= current_led + 1;
			current_io_cs_n <= next_io_cs_n;
			current_buffer_cs_n <= next_buffer_cs_n;
		end if;
	end process reg;	

	FPGA_LED_GREEN_N <= '0';
	FPGA_LED_RED_N <= current_led(25);

end BEH;
