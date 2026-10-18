/**
 * Lab Developer Community - Global Supabase Public Configuration
 * 
 * NOTE ON SECURITY:
 * The Supabase 'anon' public key and project URL are explicitly designed to be public
 * in client-side applications. Database write protection and privilege separation
 * are strictly enforced by PostgreSQL Row Level Security (RLS) on the server.
 * Never place 'service_role' keys here.
 */

window.LAB_SUPABASE_URL = window.LAB_SUPABASE_URL || "";
window.LAB_SUPABASE_ANON_KEY = window.LAB_SUPABASE_ANON_KEY || "";
