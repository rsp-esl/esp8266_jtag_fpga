------------------------------------------------------------------------------
-- Author: RSP @ Embedded System Lab (ESL), KMUTNB, Thailand
-- Create Date: 7 May 2014
-- Target Board: Mojo v3 (XC6SLX9 TQG144-2)
------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity top is
  port( CLK   : in std_logic;
        RST_N : in std_logic;
        LEDS  : out std_logic_vector( 7 downto 0 ) );
end top;

architecture Behavioral of top is
  constant COUNT_WIDTH : integer := 22;
  signal count : unsigned( COUNT_WIDTH-1 downto 0 ) := (others => '0');
  signal dir   : std_logic := '0'; 
  signal tick  : std_logic := '0'; 
  signal reg   : std_logic_vector( 7 downto 0 );

begin
  P1: process (RST_N,CLK)
  begin
    if RST_N = '0' then
       count <= (others => '0');
    elsif rising_edge(CLK) then
       count <= count + 1;
       if count = TO_UNSIGNED(2**COUNT_WIDTH-1,count'length) then
          tick <= '1';
       else
          tick <= '0';
       end if;
    end if;
  end process;
 
  P2: process (RST_N,CLK)
  begin
    if RST_N = '0' then
      reg(reg'range) <= (others => '0'); 
      reg(0) <= '1';
      dir <= '0';
    elsif rising_edge(CLK) then
      if tick = '1' then
        if dir = '0' and reg(reg'left-1) = '1' then
           dir <= '1';
        elsif dir = '1' and reg(1) = '1' then
           dir <= '0';
        end if;
        if dir = '0' then
          reg <= reg(reg'left-1 downto 0) & reg(reg'left);
        else
          reg <= reg(0) & reg(reg'left downto 1);
        end if;
      end if;
    end if;
  end process;
  
  LEDS <= reg;
end Behavioral;
------------------------------------------------------------------------------
