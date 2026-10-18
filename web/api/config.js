/**
 * Vercel Serverless Function: Exposes public Supabase connection config
 * Reads PUBLIC_SUPABASE_URL and PUBLIC_SUPABASE_ANON_KEY from Vercel Environment Variables
 */

module.exports = (req, res) => {
  // Allow caching so it doesn't incur unnecessary invocations
  res.setHeader('Cache-Control', 'public, s-maxage=3600, stale-while-revalidate=86400');
  res.setHeader('Content-Type', 'application/json');

  const url = process.env.PUBLIC_SUPABASE_URL || process.env.SUPABASE_URL || '';
  const anonKey = process.env.PUBLIC_SUPABASE_ANON_KEY || process.env.SUPABASE_ANON_KEY || '';

  res.status(200).json({ url, anonKey });
};
